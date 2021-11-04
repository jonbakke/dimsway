# dimsway
This dims inactive windows in Sway -- similar to how I used to use `picom` or `compton` in X.

# Example
To set focused windows at 100% opacity, unfocused at 70%:
```
dimsway -f 1.0 -u 0.7
```

To change opacity, e.g. when moving from light theme to dark:
```
killall dimsway
dimsway -f 1.0 -u 0.95
```

# License
MIT. Have at it, friends.

# Quality
This is the result of an afternoon project of a novice, out-of-work programmer. You have been warned.

# Install
Requires `libjson-c` development files, aka `libjson-c-dev` on Debian.

```
cc -O2 -o dimsway dimsway.c -ljson-c
cp dimsway ~/bin
```

Customize as appropriate, of course.
