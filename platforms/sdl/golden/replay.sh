#!/bin/bash
# Replays every golden .keys windowless on the simulated clock against a fresh copy of sdcard/ and compares with the stored PNGs.
# usage: platforms/sdl/golden/replay.sh OUTDIR [BINARY]   (from any directory; BINARY defaults to build-host/tracker-sdl)
# Prints "same <name>" or "DIFF <name>" per output file; exit status 1 if any differ.
set -u
R=$(cd "$(dirname "$0")/../../.." && pwd); OUT=$(mkdir -p "$1" && cd "$1" && pwd); BIN=${2:-$R/build-host/tracker-sdl}
case $BIN in /*) ;; *) BIN=$PWD/$BIN ;; esac
status=0
for k in "$R"/platforms/sdl/golden/*.keys; do
  W=$(mktemp -d); cp -R "$R/sdcard/" "$W/sdcard"
  args=$(sed -e "s#platforms/sdl/golden/#$OUT/#g" -e "s#sdcard/no-such-dir#$W/no-such-dir#" -e "s#--sd sdcard#--sd $W/sdcard#" "$k")
  (cd "$R" && "$BIN" $args --no-window --sim-clock >/dev/null 2>&1)
  rm -rf "$W"
  for p in $(grep -o "$OUT/[^ ]*\.png" <<<"$args"); do
    n=$(basename "$p")
    if cmp -s "$p" "$R/platforms/sdl/golden/$n"; then echo "same $n"; else echo "DIFF $n"; status=1; fi
  done
done
exit $status
