#!/bin/sh
# Headless screenshot of the app on the Brick, driven by a --keys script. Never touches the screen or the
# real SD root: binary and a copy of the SD root run from /tmp/pbshot, the PNG comes back to OUT.
#   platforms/sdl/devtests/brick-shot.sh 'KEYS' OUT.png [AT_MS, default 3000] [PAK, default PolyBrik] [HOST, default $BRICK_HOST or brick]
# BIN=path/to/tracker-sdl runs that local binary instead of the pak's (stripped copy sent to /tmp).
# Example (synth Instrument page in the 32-bit app): brick-shot.sh '200:fn2,700:instr:1500,1000:up,1300:up' synth.png
set -eu
KEYS="$1"; OUT="$2"; AT="${3:-3000}"; PAK="${4:-PolyBrik}"; HOST="${5:-${BRICK_HOST:-brick}}"
REMOTE_BIN=/mnt/SDCARD/Tools/tg5040/$PAK.pak/tracker-sdl
if [ -n "${BIN:-}" ]; then
  ssh -o ConnectTimeout=5 "$HOST" "mkdir -p /tmp/pbbin"; scp -q "$BIN" "$HOST:/tmp/pbbin/tracker-sdl"; REMOTE_BIN=/tmp/pbbin/tracker-sdl
fi
ssh -o ConnectTimeout=5 "$HOST" "set -e; ps | grep -q '[t]racker-sdl' && { echo 'the pak is running, quit it first'; exit 1; }
  rm -rf /tmp/pbshot; mkdir -p /tmp/pbshot; cp -R /mnt/SDCARD/$PAK /tmp/pbshot/sd; cp $REMOTE_BIN /tmp/pbshot/tracker-sdl
  cd /tmp/pbshot; LD_LIBRARY_PATH=/usr/trimui/lib:/usr/lib ./tracker-sdl --sd /tmp/pbshot/sd --no-audio --no-window --sim-clock --from-ready \
    --keys '$KEYS' --screenshot /tmp/pbshot/shot.png --screenshot-at $AT --quit-after $((AT + 200)) | tail -1"
scp -q "$HOST:/tmp/pbshot/shot.png" "$OUT"
ssh "$HOST" "rm -rf /tmp/pbshot /tmp/pbbin"
echo "$OUT"
