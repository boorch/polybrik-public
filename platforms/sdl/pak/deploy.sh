#!/bin/sh
# Stages PolyBrik64.pak (the 64-bit app; PolyBrik is the 32-bit app) from a cross build and copies it to the device over SSH.
#   platforms/sdl/pak/deploy.sh [build dir, default build-brick] [ssh host, default $BRICK_HOST or brick]
# Then launch "PolyBrik64" from the NextUI Tools menu (never kill nextui.elf over SSH).
# The firmware's SD root on the device is /mnt/SDCARD/PolyBrik64; the pak's launch.sh copies the factory content there (missing files only)
# once, when the device has no Samples folder yet.
set -eu
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD="${1:-$ROOT/build-brick}"
HOST="${2:-${BRICK_HOST:-brick}}"   # an SSH host or alias for the device
PAK="$BUILD/pak/PolyBrik64.pak"
REMOTE="/mnt/SDCARD/Tools/tg5040/PolyBrik64.pak"
STRIP="${STRIP:-aarch64-unknown-linux-gnu-strip}"

[ -x "$BUILD/tracker-sdl" ] || { echo "no $BUILD/tracker-sdl (build with cmake/tg5040-aarch64.cmake first)"; exit 1; }
rm -rf "$PAK"; mkdir -p "$PAK"
cp "$ROOT/platforms/sdl/pak/launch.sh" "$PAK/"
"$STRIP" -o "$PAK/tracker-sdl" "$BUILD/tracker-sdl"
chmod +x "$PAK/launch.sh" "$PAK/tracker-sdl"
# Factory content travels inside the pak; launch.sh copies missing files to the SD root (never overwrites).
cp -R "$ROOT/platforms/sdl/pak/content" "$PAK/content"
(cd "$PAK/content" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 md5 -r | md5 -q) > "$PAK/content.version"   # names and bytes
ls -la "$PAK"

ssh -o ConnectTimeout=5 "$HOST" "echo device ok" || { echo "cannot reach $HOST"; exit 1; }
ssh "$HOST" "mkdir -p '$REMOTE' /mnt/SDCARD/PolyBrik64"
(cd "$PAK" && COPYFILE_DISABLE=1 tar cf - .) | ssh "$HOST" "cd '$REMOTE' && tar xf -"
ssh "$HOST" "ls -la '$REMOTE' /mnt/SDCARD/PolyBrik64; sync"
echo "deployed: launch PolyBrik64 from the NextUI Tools menu; log at /mnt/SDCARD/.userdata/tg5040/logs/PolyBrik64.txt"
