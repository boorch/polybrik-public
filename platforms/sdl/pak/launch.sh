#!/bin/sh
# PolyBrik / PolyBrik64 pak launcher for NextUI on the TrimUI Brick / Brick Pro (tg5040), with the
# usual NextUI pak conventions (LD_LIBRARY_PATH order, stay_awake, governor restore).
# Lives at /mnt/SDCARD/Tools/tg5040/<pak>.pak/launch.sh next to tracker-sdl; log in $LOGS_PATH/<pak>.txt.
PAK_DIR="$(dirname "$0")"
PAK_NAME="$(basename "$PAK_DIR" .pak)"
LOGS_PATH="${LOGS_PATH:-/tmp}"
mkdir -p "$LOGS_PATH" 2>/dev/null
exec >>"$LOGS_PATH/$PAK_NAME.txt" 2>&1
echo "=== $(date) $PAK_NAME start (DEVICE=$DEVICE PLATFORM=$PLATFORM)"
cd "$PAK_DIR" || exit 1

# SD root for the firmware: projects, samples and the workspace live here and survive pak updates.
# Named after the pak: PolyBrik.pak (32-bit, synths) uses /mnt/SDCARD/PolyBrik, PolyBrik64.pak (64-bit)
# uses /mnt/SDCARD/PolyBrik64, so the two never share a workspace.
SD="${SDCARD_PATH:-/mnt/SDCARD}/$PAK_NAME"
mkdir -p "$SD"

# Factory content shipped in the pak (content/), applied once per content version:
# - Sample packs (content/Samples/<pack>, e.g. Samples/PolyBrik Factory) belong to the app: the folder is replaced
#   whole, so fixed or removed factory samples reach every user. Keep your own samples outside those folders.
# - Everything else (synth patches in Patches/<ENGINE>/, where users save their own patches too, and license
#   files) is copied only where the file does not exist yet: nothing is ever overwritten or deleted there.
if [ -d "$PAK_DIR/content" ] && [ "$(cat "$SD/.factory-content" 2>/dev/null)" != "$(cat "$PAK_DIR/content.version" 2>/dev/null)" ]; then
    for d in "$PAK_DIR"/content/Samples/*/; do
        [ -d "$d" ] || continue
        rel="Samples/$(basename "$d")"
        rm -rf "$SD/$rel"
        mkdir -p "$SD/Samples"
        cp -R "$PAK_DIR/content/$rel" "$SD/$rel"
    done
    (cd "$PAK_DIR/content" && find . -type f ! -path './Samples/*') | while IFS= read -r f; do
        [ -e "$SD/$f" ] || { mkdir -p "$SD/$(dirname "$f")"; cp "$PAK_DIR/content/$f" "$SD/$f"; }
    done
    cp "$PAK_DIR/content.version" "$SD/.factory-content"
fi

export LD_LIBRARY_PATH="/usr/trimui/lib:/usr/lib:$PAK_DIR/lib:$LD_LIBRARY_PATH"
# hw:0,0 takes 44100 directly; the ALSA "default" is a dmix fixed at 48000 that resamples
# on the device. SDL's ALSA backend opens $AUDIODEV when it is set.
export AUDIODEV="${AUDIODEV:-hw:0,0}"

echo 1 > /tmp/stay_awake
ORIG_GOV="$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)"
cleanup() {
    rm -f /tmp/stay_awake
    if [ -n "$ORIG_GOV" ]; then
        for c in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo "$ORIG_GOV" > "$c" 2>/dev/null; done
    fi
    cat /dev/zero > /dev/fb0 2>/dev/null
}
trap cleanup EXIT INT TERM HUP QUIT
for c in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance > "$c" 2>/dev/null; done

# TRACKER_ARGS: extra flags for a debug run, for example "--log /tmp/t.log --quit-after 60000".
# An empty file named generic-r4 in the SD root makes R4 the screen-key mode on every page (debugging).
[ -e "$SD/generic-r4" ] && TRACKER_ARGS="$TRACKER_ARGS --generic-r4"
# An empty file named no-scale draws the 800x480 canvas 1:1 instead of stretched to the screen width.
[ -e "$SD/no-scale" ] && TRACKER_ARGS="$TRACKER_ARGS --no-scale"
while :; do
    ./tracker-sdl --sd "$SD" --fullscreen $TRACKER_ARGS
    rc=$?
    echo "exit code $rc"
    [ "$rc" = "42" ] || break   # 42 = PlatformImpl::resetMCU(): the firmware asked to restart itself
done
