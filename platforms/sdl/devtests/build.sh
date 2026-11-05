#!/bin/sh
# Rebuilds every device test binary into ./out. Needs zig 0.16 and an aarch64 TrimUI sysroot (SYSROOT=...).
set -eu
cd "$(dirname "$0")"
SYSROOT="${SYSROOT:?set SYSROOT to an aarch64 TrimUI sysroot}"
mkdir -p out
# 32-bit static probe. zig ignores -mthumb with -target arm; the thumb target makes main Thumb-2.
zig cc -target thumb-linux-musleabihf -mcpu=cortex_a53 -static -O1 probe32.c -o out/probe32
# static aarch64 helpers
zig cc -target aarch64-linux-musl -static -O2 evdump.c -o out/evdump
zig cc -target aarch64-linux-musl -static -O2 fbinfo.c -o out/fbinfo
# SDL test, dynamic against libSDL2-2.0.so.0 (device provides /usr/trimui/lib/libSDL2-2.0.so.0)
zig cc --target=aarch64-linux-gnu.2.33 -O2 -D_REENTRANT \
  -isystem "$SYSROOT/usr/include/SDL2" -isystem "$SYSROOT/usr/include" -isystem "$SYSROOT/usr/include/aarch64-linux-gnu" \
  -L "$SYSROOT/usr/lib/aarch64-linux-gnu" sdltest.c -o out/sdltest -lSDL2 -Wl,--allow-shlib-undefined
file out/*
