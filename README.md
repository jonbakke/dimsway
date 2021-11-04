# dimsway
This dims inactive windows in Sway -- similar to how I used to use `picom` or `compton` in X.

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
