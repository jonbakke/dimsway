# dimsway
This dims inactive windows in Sway -- similar to how I used to use `picom` or `compton` in X.

# Example
To set unfocused windows at 70% opacity:
```
dimsway 0.7
```

To change opacity, e.g. when moving from light theme to dark:
```
# Increase opacity:
kill -SIGUSR1 $(pgrep dimsway)
# Decrease opacity:
kill -SIGUSR2 $(pgrep dimsway)
```

# License
MIT. Have at it, friends.

# Quality
Lackasaisical at best, but it serves me well.

# Install
Requires `libjson-c` development files, aka `libjson-c-dev` on Debian.

Installs to `~/bin` by default. Edit `Makefile` to select a different location.


```
make install
```
