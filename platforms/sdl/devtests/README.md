# Device tests (TrimUI Brick Pro)

Throwaway probes for the device: 32-bit support, input, framebuffer, SDL display, audio and joystick.

| File | What it is |
|---|---|
| `probe32.c` | 32-bit static ARM probe: checks compat, DSP (qadd16, smlad) and VFP. |
| `evdump.c` | Static aarch64. Prints every input_event from all `/dev/input/event*` with names. `evdump [seconds] [dev...]`. |
| `fbinfo.c` | Static aarch64. Prints FBIOGET_VSCREENINFO / FBIOGET_FSCREENINFO for `/dev/fb0`. |
| `sdltest.c` | Dynamic aarch64 against `libSDL2-2.0.so.0`. Display mode, renderer, audio obtained spec, joystick, event log. `sdltest [seconds]`. |
| `launch.sh` | Pak launcher for `/mnt/SDCARD/Tools/tg5040/SdlTest.pak/`. |
| `build.sh` | Rebuilds everything into `out/` (ignored by `devtests/.gitignore`). |

Pro stick probe: `sdltest` prints every
`axis N = value`, `evdump` the raw ABS codes (0 X, 1 Y, 3 RX, 4 RY); either one gives the rest values
and ranges the stick constants in `../InputMap.h` are tuned against.

## Build

```sh
./build.sh          # needs zig 0.16 and SYSROOT set to an aarch64 TrimUI sysroot
```

Exact commands it runs:

```sh
zig cc -target thumb-linux-musleabihf -mcpu=cortex_a53 -static -O1 probe32.c -o out/probe32
zig cc -target aarch64-linux-musl -static -O2 evdump.c -o out/evdump
zig cc -target aarch64-linux-musl -static -O2 fbinfo.c -o out/fbinfo
zig cc --target=aarch64-linux-gnu.2.33 -O2 -D_REENTRANT \
  -isystem "$SYSROOT/usr/include/SDL2" -isystem "$SYSROOT/usr/include" -isystem "$SYSROOT/usr/include/aarch64-linux-gnu" \
  -L "$SYSROOT/usr/lib/aarch64-linux-gnu" sdltest.c -o out/sdltest -lSDL2 -Wl,--allow-shlib-undefined
```

Note: `zig cc -target arm-linux-musleabihf -mthumb` silently builds `main` as ARM (A32), checked with
`arm-none-eabi-objdump`. Use `-target thumb-linux-musleabihf` to get Thumb-2.

## Deploy

```sh
scp out/probe32 out/evdump out/fbinfo brick:/tmp/
ssh brick 'mkdir -p /mnt/SDCARD/Tools/tg5040/SdlTest.pak'
scp out/sdltest launch.sh brick:/mnt/SDCARD/Tools/tg5040/SdlTest.pak/
```
