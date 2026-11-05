#!/bin/sh
# SdlTest.pak/launch.sh (pak launcher, trimmed for the test)
PAK_DIR="$(dirname "$0")"; PAK_NAME="$(basename "$PAK_DIR" .pak)"
mkdir -p "$LOGS_PATH" 2>/dev/null; exec >>"$LOGS_PATH/$PAK_NAME.txt" 2>&1
echo "=== $(date) $PAK_NAME start (DEVICE=$DEVICE PLATFORM=$PLATFORM)"
cd "$PAK_DIR" || exit 1
export LD_LIBRARY_PATH="$PAK_DIR/lib:/usr/trimui/lib:$LD_LIBRARY_PATH"
./sdltest 20
echo "exit code $?"
cat /dev/zero > /dev/fb0 2>/dev/null
