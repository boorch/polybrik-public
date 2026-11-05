#!/bin/sh
# Stages PolyBrik.pak (the 32-bit app with the Polyend synths, the release target)
# and, unless --stage-only, copies it to the device over SSH. It sits next to the 64-bit PolyBrik64.pak and never touches it.
#   platforms/sdl/pak/deploy-t32.sh [--stage-only] [build dir, default build-t32app/app] [ssh host, default $BRICK_HOST or brick]
# Then launch "PolyBrik" from the NextUI Tools menu (never kill nextui.elf over SSH).
# SD root on the device: /mnt/SDCARD/PolyBrik (its own: T32 projects have three more instrument
# slots and stereo samples). The pak's launch.sh copies the factory content there (missing files only).
set -eu
STAGE_ONLY=0
[ "${1:-}" = "--stage-only" ] && { STAGE_ONLY=1; shift; }
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD="${1:-$ROOT/build-t32app/app}"
HOST="${2:-${BRICK_HOST:-brick}}"   # an SSH host or alias for the device
NAME=PolyBrik
PAK="$ROOT/build-t32app/pak/$NAME.pak"
REMOTE="/mnt/SDCARD/Tools/tg5040/$NAME.pak"
SDROOT="/mnt/SDCARD/$NAME"
STRIP="${STRIP:-${T32_PREFIX:-arm-unknown-linux-gnueabihf-}strip}"

[ -x "$BUILD/tracker-sdl" ] || { echo "no $BUILD/tracker-sdl (build with cmake/t32-armhf.cmake first)"; exit 1; }
file "$BUILD/tracker-sdl" | grep -q "ELF 32-bit.*ARM.*statically linked" || { echo "$BUILD/tracker-sdl is not a static armhf binary"; exit 1; }
rm -rf "$PAK"; mkdir -p "$PAK"
cp "$ROOT/platforms/sdl/pak/launch.sh" "$PAK/"
"$STRIP" -o "$PAK/tracker-sdl" "$BUILD/tracker-sdl"
chmod +x "$PAK/launch.sh" "$PAK/tracker-sdl"
# Factory content travels inside the pak; launch.sh copies missing files to the SD root (never overwrites).
cp -R "$ROOT/platforms/sdl/pak/content" "$PAK/content"
(cd "$PAK/content" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 md5 -r | md5 -q) > "$PAK/content.version"   # names and bytes
ls -la "$PAK"
[ "$STAGE_ONLY" = 1 ] && { echo "staged $PAK (not deployed)"; exit 0; }

ssh -o ConnectTimeout=5 "$HOST" "echo device ok" || { echo "cannot reach $HOST"; exit 1; }
ssh "$HOST" "mkdir -p '$REMOTE' '$SDROOT'"
(cd "$PAK" && COPYFILE_DISABLE=1 tar cf - .) | ssh "$HOST" "cd '$REMOTE' && tar xf -"
ssh "$HOST" "ls -la '$REMOTE' '$SDROOT'; sync"
echo "deployed: launch $NAME from the NextUI Tools menu; log at /mnt/SDCARD/.userdata/tg5040/logs/$NAME.txt"
