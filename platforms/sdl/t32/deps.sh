#!/bin/sh
# Builds the T32 dependencies (cmake/t32-armhf.cmake reads them from $OUT/deps):
#   glibc/   Debian buster libc6-dev 2.28 armhf static libs and crt files. The Homebrew crosstool
#            sysroot is glibc 2.28 as well but configured for Linux 5.10 ("FATAL: kernel too old" on
#            the Brick's 4.9.191); Debian's is configured for 3.2.
#   span/    GCC 15's <span> with its two data members swapped back to the GCC 10 order
#            (_M_extent, then _M_ptr). The archives were built by GCC 10.3 and take std::span by
#            value (Synth::setup, renderAudioBuffer, FileSystem::read/write, InstrumentVoice);
#            GCC 11 reversed the order, so a GCC 15 caller hands them pointer and size swapped.
#   prefix/  static alsa-lib 1.1.9 (the device's own ALSA is 1.1.0; config read from the device's
#            /usr/share/alsa) and static SDL2 2.30.8 (the device's version), dummy video only,
#            evdev joystick without udev, ALSA audio linked in, no X11/Wayland/GL/HIDAPI/dlopen.
#            Both are built without -fshort-enums (see ARCH below).
# Sources are downloaded into $OUT/dl and unpacked into $OUT/src (gitignored build dir).
#   [T32_PREFIX=<compiler prefix with trailing dash>] [T32_GLIBC=debian|sysroot] platforms/sdl/t32/deps.sh [out dir]
# T32_PREFIX defaults to the Homebrew arm-unknown-linux-gnueabihf- (GCC 15); T32_GLIBC=sysroot skips the
# Debian libc for a toolchain whose own glibc already supports Linux 4.9. Out dir default build-t32app.
set -eu
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="${1:-$ROOT/build-t32app}"
mkdir -p "$OUT" && OUT="$(cd "$OUT" && pwd)"   # absolute: the alsa build cds into its source
DEPS="$OUT/deps"
P="${T32_PREFIX:-arm-unknown-linux-gnueabihf-}"
TRIPLE="$(${P}gcc -dumpmachine)"
# No -fshort-enums for the C libraries: alsa-lib casts enum pointers to unsigned int* (pcm.c), so a
# short-enum alsa-lib or a short-enum SDL calling it writes 4 bytes into 1-byte enums. Both stay
# int-enum; only the SDL public API crosses into our short-enum code.
ARCH="-march=armv8-a -mthumb -mfloat-abi=hard -mfpu=fp-armv8"
# Paths inside the libraries relative to the source folders, so the binary carries no home directory.
ARCH="$ARCH -ffile-prefix-map=$OUT/src/="
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
mkdir -p "$OUT/dl" "$OUT/src" "$DEPS/glibc" "$DEPS/span" "$DEPS/prefix"

fetch() {   # url sha256
    f="$OUT/dl/$(basename "$1")"
    [ -f "$f" ] || curl -fL --retry 3 -o "$f" "$1"
    echo "$2  $f" | shasum -a 256 -c - >/dev/null || { echo "checksum mismatch: $f"; exit 1; }
}
fetch https://archive.debian.org/debian/pool/main/g/glibc/libc6-dev_2.28-10+deb10u1_armhf.deb \
    1fa45a3bfd60b4c27751ab8d4be159984a035560127866648c8708c237b75fec
fetch https://www.alsa-project.org/files/pub/lib/alsa-lib-1.1.9.tar.bz2 \
    488373aef5396682f3a411a6d064ae0ad196b9c96269d0bb912fbdeec94b994b
fetch https://github.com/libsdl-org/SDL/releases/download/release-2.30.8/SDL2-2.30.8.tar.gz \
    380c295ea76b9bd72d90075793971c8bcb232ba0a69a9b14da4ae8f603350058

# ---- glibc --------------------------------------------------------------------------------------
if [ "${T32_GLIBC:-debian}" = debian ] && [ ! -f "$DEPS/glibc/libc.a" ]; then
    rm -rf "$OUT/src/deb" && mkdir -p "$OUT/src/deb"
    (cd "$OUT/src/deb" && ar x "$OUT/dl/libc6-dev_2.28-10+deb10u1_armhf.deb" && tar xf data.tar.*)
    cp "$OUT/src/deb/usr/lib/arm-linux-gnueabihf/"*.a "$OUT/src/deb/usr/lib/arm-linux-gnueabihf/"*.o "$DEPS/glibc/"
fi

# ---- span ---------------------------------------------------------------------------------------
GXXVER="$(${P}g++ -dumpfullversion)"
GXXINC="$(dirname "$(${P}g++ -print-libgcc-file-name)")/../../../../$TRIPLE/include/c++/$GXXVER"
rm -f "$DEPS/span/span"
if [ "${GXXVER%%.*}" -ge 11 ]; then
    awk '
    /^      pointer _M_ptr;$/ { held = $0; next }
    held != "" && /__extent_storage<extent> _M_extent;$/ { print; print held; held = ""; swapped = 1; next }
    { print }
    END { if (!swapped) exit 1 }
' "$GXXINC/span" > "$DEPS/span/span" || { echo "span: member lines not found in $GXXINC/span"; exit 1; }
fi

# ---- alsa-lib -----------------------------------------------------------------------------------
if [ ! -f "$DEPS/prefix/lib/libasound.a" ]; then
    rm -rf "$OUT/src/alsa-lib-1.1.9" && tar xjf "$OUT/dl/alsa-lib-1.1.9.tar.bz2" -C "$OUT/src"
    # GCC 15 defaults to C23; alsa-lib 1.1.9 is C99 code and relies on common symbols (GCC 10+ is -fno-common). --prefix=/usr keeps the compiled-in config
    # dir at /usr/share/alsa, where the device's alsa.conf lives; DESTDIR installs into prefix/.
    (cd "$OUT/src/alsa-lib-1.1.9" &&
        ./configure --host=$TRIPLE --prefix=/usr CC=${P}gcc AR=${P}ar RANLIB=${P}ranlib STRIP=${P}strip CFLAGS="-O2 -std=gnu11 -fcommon $ARCH" \
            --enable-static --disable-shared --with-libdl=no --disable-python --disable-alisp \
            --disable-topology --disable-old-symbols --with-debug=no >/dev/null &&
        make -j"$JOBS" >/dev/null &&
        make install DESTDIR="$OUT/src/alsa-install" >/dev/null)
    mkdir -p "$DEPS/prefix/lib" "$DEPS/prefix/include"
    cp "$OUT/src/alsa-install/usr/lib/libasound.a" "$DEPS/prefix/lib/"
    cp -R "$OUT/src/alsa-install/usr/include/alsa" "$DEPS/prefix/include/"
fi

# ---- SDL2 ---------------------------------------------------------------------------------------
if [ ! -f "$DEPS/prefix/lib/libSDL2.a" ]; then
    rm -rf "$OUT/src/SDL2-2.30.8" "$OUT/src/sdl-build" && tar xzf "$OUT/dl/SDL2-2.30.8.tar.gz" -C "$OUT/src"
    cmake -S "$OUT/src/SDL2-2.30.8" -B "$OUT/src/sdl-build" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm \
        -DCMAKE_C_COMPILER=${P}gcc -DCMAKE_C_FLAGS="$ARCH" \
        -DCMAKE_INSTALL_PREFIX="$DEPS/prefix" \
        -DALSA_INCLUDE_DIR="$DEPS/prefix/include" -DALSA_LIBRARY="$DEPS/prefix/lib/libasound.a" \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL2_DISABLE_SDL2MAIN=ON -DSDL_LOADSO=OFF \
        -DSDL_VIDEO=ON -DSDL_DUMMYVIDEO=ON -DSDL_OFFSCREEN=OFF -DSDL_X11=OFF -DSDL_WAYLAND=OFF \
        -DSDL_KMSDRM=OFF -DSDL_RPI=OFF -DSDL_VIVANTE=OFF -DSDL_DIRECTFB=OFF -DSDL_OPENGL=OFF \
        -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF -DSDL_RENDER=ON \
        -DSDL_AUDIO=ON -DSDL_ALSA=ON -DSDL_ALSA_SHARED=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF \
        -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_OSS=OFF -DSDL_ESD=OFF -DSDL_ARTS=OFF -DSDL_NAS=OFF \
        -DSDL_LIBSAMPLERATE=OFF -DSDL_DISKAUDIO=OFF \
        -DSDL_JOYSTICK=ON -DSDL_HIDAPI=OFF -DSDL_HIDAPI_JOYSTICK=OFF -DSDL_VIRTUAL_JOYSTICK=OFF \
        -DSDL_LIBUDEV=OFF -DSDL_DBUS=OFF -DSDL_IBUS=OFF -DSDL_FCITX=OFF -DSDL_HAPTIC=OFF \
        -DSDL_SENSOR=OFF -DSDL_POWER=OFF -DSDL_RPATH=OFF -DSDL_WERROR=OFF >/dev/null
    cmake --build "$OUT/src/sdl-build" -j"$JOBS" >/dev/null
    cmake --install "$OUT/src/sdl-build" >/dev/null
fi
ls -la "$DEPS/prefix/lib"
echo "T32 deps ready in $DEPS"
