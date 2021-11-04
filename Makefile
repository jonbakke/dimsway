CC=gcc
OPT=-O2
LIBS=-ljson-c
BIN_PATH="$(HOME)/bin"

dimsway: dimsway.c Makefile
	$(CC) $(OPT) -o dimsway dimsway.c $(LIBS)

install: dimsway
	mkdir -p $(BIN_PATH)
	cp -f dimsway $(BIN_PATH)
