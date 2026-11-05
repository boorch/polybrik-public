#!/bin/bash
# Builds synthhelper: a static 32-bit ARM Linux binary that links the
# closed synth archives unmodified. Output: build-t32/synthhelper.
#
# Route B: the bare-metal GNU Arm Embedded Toolchain 10.3-2021.10, the exact compiler and
# libstdc++ the archives were built with, plus our own Linux _start and syscalls
# (linux_syscalls.c). Route A (a glibc cross GCC 15) links but crashes: GCC 11 changed the
# member order of std::span, which the archives' public API passes by value.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
TCE="$ROOT/tracker-ce"
C="$TCE/core"
OUT="${OUT:-$ROOT/build-t32}"
BIN="${ARM_NONE_EABI:-/opt/gcc-arm-none-eabi-10.3-2021.10/bin}"
mkdir -p "$OUT"

# The archives' own target (readelf -A): ARMv7E-M Thumb-2, FPv5-D16, hard float, short enums.
# It also selects the thumb/v7e-m+dp/hard newlib and libstdc++ multilib. An A53 runs it in AArch32.
ARCH="-mthumb -march=armv7e-m+fp.dp -mfloat-abi=hard -mfp16-format=ieee -fshort-enums"
CXXFLAGS="$ARCH -fno-exceptions -fno-rtti -std=gnu++20 -O2 -Wall -Wno-unused-parameter -Wno-psabi \
  -ffunction-sections -fdata-sections -DPLATFORM_SDL -DTRACKER_MINI -DFEATURE_SYNTH \
  -include cstdint -include cassert"
INC="-I$C/include -I$C/config -I$C/source -I$C/synth -I$C/libs -I$C/libs/Audio
  -I$C/libs/ElapsedMillis -I$C/libs/Audio/utility -I$C/FileManager -I$C/AudioEngine
  -I$C/AudioEngine/ConstantValues -I$C/AudioEngine/MasterEnhancer -I$C/interface -I$C/effector
  -I$C/Exporter -I$C/libs/FT812 -I$C/libs/fonts -I$C/libs/NewDisplay -I$C/libs/Audio/Polyverb
  -I$TCE/util/include -I$TCE/util/3rdparty -I$TCE/synth/include"

SRCS=(
  "$C/synth/Synth.cpp" "$C/synth/MidiAdapter.cpp" "$C/synth/FileSystemAdapter.cpp"
  "$C/synth/registry.cpp" "$C/synth/engines/ChiptuneSynthVoice.cpp"
  "$C/libs/Audio/synth_PolySynth.cpp" "$C/libs/Audio/AudioStream.cpp"
  "$TCE/util/3rdparty/fmt/format.cc"
  "$HERE/firmware_stubs.cpp" "$HERE/synthhelper.cpp"
)
OBJS=()
for s in "${SRCS[@]}"; do
  o="$OUT/$(basename "${s%.*}").o"
  "$BIN/arm-none-eabi-g++" $CXXFLAGS $INC -c "$s" -o "$o"
  OBJS+=("$o")
done
"$BIN/arm-none-eabi-gcc" $ARCH -O2 -fomit-frame-pointer -c "$HERE/linux_syscalls.c" -o "$OUT/linux_syscalls.o"
OBJS+=("$OUT/linux_syscalls.o")

# -nostartfiles: our _start replaces newlib's bare-metal crt0. nosys.specs supplies ENOSYS stubs
# for the hooks linux_syscalls.c leaves out. Text at 0x10000 with 4 KB pages so the kernel can map it.
"$BIN/arm-none-eabi-g++" $ARCH -static -nostartfiles --specs=nosys.specs \
  -Wl,--gc-sections -Wl,-z,max-page-size=4096 -Wl,-Ttext-segment=0x10000 -Wl,-e,_start \
  "${OBJS[@]}" \
  -Wl,--start-group "$TCE/synth/lib/libPolySynth.a" "$TCE/synth/lib/libPolyDSP.a" -Wl,--end-group \
  -lm -o "$OUT/synthhelper"
file "$OUT/synthhelper"
