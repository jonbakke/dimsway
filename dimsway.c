/* dimsway - dim inactive windows in Sway */
/* copyright 2021 Jonathan Bakke -- MIT license offered */

/* This software was an afternoon project. It certainly has many bugs. */

/* Set UNFOCUSED to determine the default opacity of such windows. */
/* The value for UNFOCUSED can also be set as a command-line argument,
 * or SIGUSR1 to increase and SIGUSR2 to decrease value by INCREMENT */

#define UNFOCUSED 0.95
#define INCREMENT 0.05

#include <json-c/json.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

enum sway_msg_type {
	RUN_COMMAND = 0,
	GET_WORKSPACES,
	SUBSCRIBE,
	GET_OUTPUTS,
	GET_TREE,
	GET_MARKS,
	GET_BAR_CONFIG,
	GET_VERSION,
	GET_BINDING_MODES,
	GET_CONFIG,
	SEND_TICK,
	SYNC,
	GET_BINDING_STATE,
	GET_INPUTS = 100,
	GET_SEATS,
};

const char usage[] = "Usage: %s [opacity]\n"
	"\twhere opacity is in the range [0.0, 1.0],\n"
	"\twith a default value of %.2f.\n";
volatile double dim_focused;
volatile double dim_unfocused;
struct sigaction sa;

void
endian_copy_4_bytes(uint8_t *start, const uint32_t value)
{
	union {
		uint32_t four;
		uint8_t one[4];
	} parse;
	parse.four = value;
	for (int i = 0; i < 4; ++i)
		start[i] = parse.one[i];
}

void
change_opacity(int sig)
{
	double delta;
#ifdef INCREMENT
	delta = INCREMENT;
#else
	delta = 0.05;
#endif

	switch (sig) {
	case SIGUSR1:
		dim_unfocused += delta;
		if (1.0 < dim_unfocused)
			dim_unfocused = 1.0;
		break;
	case SIGUSR2:
		dim_unfocused -= delta;
		if (0.0 > dim_unfocused)
			dim_unfocused = 0.0;
		break;
	default:
		break;
	}
}

/* opens a Sway socket; returns an int, or -1 on failure */
int
connect_sway_socket(void)
{
	int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (0 > socket_fd) {
		fprintf(stderr, "%s\n",
			"Could not get Sway socket file descriptor."
		);
		return -1;
	}
	const char *sway_addr_str = getenv("SWAYSOCK");
	if (NULL == sway_addr_str) {
		fprintf(stderr, "%s\n",
			"The Sway socket address, $SWAYSOCK, was empty."
		);
		return -1;
	}
	struct sockaddr_un sway_addr;
	memset(&sway_addr, 0, sizeof(struct sockaddr_un));
	sway_addr.sun_family = AF_UNIX;
	strncpy(
		sway_addr.sun_path,
		sway_addr_str,
		sizeof (sway_addr.sun_path) - 1
	);
	int ret = connect(
		socket_fd,
		(struct sockaddr*) &sway_addr,
		sizeof (sway_addr.sun_path) - 1
	);
	if (0 > ret) {
		fprintf(stderr, "%s\n",
			"Could not open Sway socket."
		);
		return -1;
	}

	return socket_fd;
}

/* leaves socket open for additional operations;
 * socket_fd may be NULL;
 * *socket_fd may be <0; if so, acquired file descriptor is written;
 * returns the socket file descriptor, or <0 on failure */
int
send_sway(
	int *socket_fd,
	enum sway_msg_type type,
	char * const message)
{
	if (NULL == socket_fd)
		return -1;

	enum { MSG_LEN_MAX = 4096 };
	const char *sway_magic = "i3-ipc";
	uint8_t msg[MSG_LEN_MAX];
	int index = 0;
	for (int i = 0; i < strlen(sway_magic); ++i)
		msg[index++] = sway_magic[i];

	/* message length */
	int msg_len;
	if (NULL == message)
		msg_len = 0;
	else
		msg_len = strlen(message);
	/* 6 for magic, 4 for len, 4 for type */
	if (MSG_LEN_MAX < msg_len + 14) {
		fprintf(stderr, "%s %d %s\n%s\n",
			"Attempted to send message longer than",
			MSG_LEN_MAX,
			"bytes. Ignoring:",
			message
		);
		return -2;
	}
	endian_copy_4_bytes(msg + index, msg_len);
	index += 4;

	/* message type */
	endian_copy_4_bytes(msg + index, type);
	index += 4;

	for (int i = 0; i < msg_len; ++i)
		msg[index++] = message[i];

	/* send query */

	int fd = -1;
	if (NULL == socket_fd)
		fd = connect_sway_socket();
	else
		fd = *socket_fd;

	if (0 > fd) {
		fd = connect_sway_socket();
		*socket_fd = fd;
	}

	if (0 <= fd)
		write(fd, msg, index);

	return fd;
}

/* returns an alloc'd string;
 * reads from socket and leaves socket open;
 * socket_fd must not be NULL;
 * if *socket_fd < 0, will open socket and replace value */
char*
get_sway(int *socket_fd)
{
	if (NULL == socket_fd)
		return NULL;

	int fd;
	if (0 <= *socket_fd) {
		fd = *socket_fd;
	} else {
		fd = connect_sway_socket();
		*socket_fd = fd;
	}
	if (0 > fd)
		return NULL;

	unsigned char ret_dump[6];
	uint32_t ret_len;
	/* read and ignore magic "i3-ipc" */
	read(fd, ret_dump, 6);
	/* read and save uint32_t length */
	read(fd, &ret_len, 4);
	/* read and discard uint32_t type */
	read(fd, ret_dump, 4);

	char *sway_str = malloc(ret_len + 1);
	if (NULL == sway_str)
		return NULL;
	sway_str[ret_len] = 0;
	read(fd, sway_str, ret_len);
	return sway_str;
}

/* for a given parent, search for a child with a given name;
 * returns pointer to child json_object or NULL if not found;
 * neither parent nor name may be NULL */
struct json_object*
get_json_child(struct json_object *parent, char *name)
{
	if (NULL == parent || NULL == name)
		return NULL;

	struct json_object *result;
	if (!json_object_object_get_ex(parent, name, &result))
		return NULL;
	return result;
}

void
set_opacity(int con_id, double opacity, int *socket_fd)
{
	enum { CMD_LEN = 1024 };
	static char command[CMD_LEN];
	snprintf(command, CMD_LEN, "%s%d%s%f",
		"[con_id=\"",
		con_id,
		"\"] opacity set ",
		opacity
	);
	send_sway(socket_fd, RUN_COMMAND, command);
	/* empty response buffer */
	free(get_sway(socket_fd));
}

int
command_failed(struct json_object *object)
{
	/* Sway might return success objects in an array */
	struct json_object *item, *success;
	if (json_object_is_type(object, json_type_array))
		item = json_object_array_get_idx(object, 0);
	else
		item = object;
	success = get_json_child(item, "success");
	if (NULL == success)
		/* item not found */
		return 0;
	if (!json_object_is_type(success, json_type_boolean))
		/* success is a different type?? */
		return 0;
	if (json_object_get_boolean(success))
		/* success: true */
		return 0;
	return 1;
}

void
subscribe_to_window_changes(void)
{
	int fd = -1;
	int last_id = -1;
	char *response_str;
	struct json_object *response_json, *change, *container, *id;

	/* set all windows translucent */
	send_sway(&fd, RUN_COMMAND,
		"for_window [tiling] opacity dim_unfocused");
	free(get_sway(&fd));

	/* get focused */
	send_sway(&fd, GET_SEATS, NULL);
	response_str = get_sway(&fd);
	response_json = json_tokener_parse(response_str);
	container = json_object_array_get_idx(response_json, 0);
	last_id = json_object_get_int(container);
	json_object_put(response_json);
	free(response_str);

	/* "window" is in a JSON array */
	send_sway(&fd, SUBSCRIBE, "[\"window\"]");
	free(get_sway(&fd));
	
	for (;;) {
		/* block between events */
		response_str = get_sway(&fd);
		response_json = json_tokener_parse(response_str);
		if (NULL == response_json || command_failed(response_json)) {
			free(response_str);
			continue;
		}
		change = get_json_child(response_json, "change");
		if (NULL == change)
			goto clear_and_continue;
		if (0 == strcmp("close", json_object_get_string(change)))
			goto clear_and_continue;
		if (0 == strcmp("new", json_object_get_string(change)))
			goto clear_and_continue;
		if (0 == strcmp("title", json_object_get_string(change)))
			goto clear_and_continue;
		container = get_json_child(response_json, "container");
		if (NULL == container)
			goto clear_and_continue;
		id = get_json_child(container, "id");
		if (NULL == id)
			goto clear_and_continue;
		if (0 <= last_id)
			set_opacity(last_id, dim_unfocused, &fd);
		last_id = json_object_get_int(id);
		set_opacity(last_id, dim_focused, &fd);
clear_and_continue:
		json_object_put(response_json);
		free(response_str);
	}
}

int
main(int argc, char **argv)
{
	char *test;
#ifdef FOCUSED
	dim_focused = FOCUSED;
#else
	dim_focused = 1.00;
#endif
#ifdef UNFOCUSED
	dim_unfocused = UNFOCUSED;
#else
	dim_unfocused = 0.95;
#endif
	const double orig_unfocused = dim_unfocused;

	if (2 <= argc) {
		dim_unfocused = strtod(argv[1], &test);
		if (
			test == argv[1] ||
			0.0 > dim_unfocused ||
			1.0 < dim_unfocused
		) {
			fprintf(stderr, usage, argv[0], orig_unfocused);
			exit(-1);
		}
	}

	sa.sa_handler = change_opacity;
	sa.sa_flags = 0;
	sa.sa_restorer = NULL;
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);

	subscribe_to_window_changes();
	return 0;
}
