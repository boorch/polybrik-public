# The 32-bit toolchain: arm-gcc10-linux-gnueabihf

PolyBrik's 32-bit binary is built with a crosstool-NG toolchain: **GCC 10.3.0, binutils 2.38, glibc 2.28, Linux 4.9 headers**, hard float, static `libc.a` and `libstdc++.a`. This folder holds what is needed to rebuild it: the crosstool-NG configuration and the patches. The steps below are the ones used for the released build, on macOS (Apple silicon, Apple clang). A full build takes about 25 minutes.

## Why this toolchain

- **glibc 2.28, minimum kernel 3.2.0.** The Brick runs Linux 4.9.191. crosstool-NG's default would set glibc's minimum kernel to the version of the kernel headers (4.9.335), which is newer than the device, and every binary would stop with "kernel too old". The config sets `CT_GLIBC_MIN_KERNEL_VERSION="3.2.0"`.
- **GCC 10.3.0.** It matches the compiler generation of Polyend's synth archives. crosstool-NG 1.29.0 only ships GCC 10.5.0, so 10.3.0 is fed in as a custom source.
- **Static linking.** The binary carries everything it needs, so it runs on the device without matching system libraries.

## Files

| File | What it is |
|---|---|
| `arm-gcc10-linux-gnueabihf.config` | the crosstool-NG `.config` used for the build. Paths point at `${HOME}/ct-work/vol/...`; change them if you work elsewhere. |
| `patches/binutils/2.38/9000-zlib-no-fdopen-macro-on-macos.patch` | macOS host fix for binutils' bundled zlib (applied by crosstool-NG as a local patch, `CT_LOCAL_PATCH_DIR`) |
| `gcc-10.3.0-macos-host.patch` | macOS host fixes for GCC 10.3.0: the same zlib fix, and `gcc/system.h` includes the C++ standard headers before `safe-ctype.h` (libc++ on macOS breaks otherwise; GCC fixed this upstream in PR111632, GCC 13) |

On a Linux host the two macOS patches should not be needed. That path has not been tried.

## Steps (macOS)

```sh
brew install crosstool-ng gnu-sed gawk grep coreutils bison flex help2man autoconf automake gperf binutils

# crosstool-NG needs a case-sensitive file system
mkdir -p ~/ct-work && cd ~/ct-work
hdiutil create -type SPARSE -fs 'Case-sensitive APFS' -size 30g -volname ctwork ctwork.sparseimage
hdiutil attach ctwork.sparseimage -mountpoint ~/ct-work/vol
mkdir -p ~/ct-work/vol/{src,gcc103,tarballs,build}

# GCC 10.3.0 from GNU, prepared by hand (crosstool-NG applies no patches to a custom source)
curl -L -o ~/ct-work/vol/src/gcc-10.3.0.tar.xz https://ftp.gnu.org/gnu/gcc/gcc-10.3.0/gcc-10.3.0.tar.xz
tar xJf ~/ct-work/vol/src/gcc-10.3.0.tar.xz -C ~/ct-work/vol/gcc103
cd ~/ct-work/vol/gcc103/gcc-10.3.0
CT=$(brew --prefix crosstool-ng)/share/crosstool-ng/packages/gcc/10.5.0
for p in "$CT"/00{00..23}-*.patch; do patch -p1 < "$p"; done   # crosstool-NG's generic GCC patches; 0024+ are AArch64-only
patch -p1 < <repo>/platforms/sdl/t32/toolchain/gcc-10.3.0-macos-host.patch

# local patch for binutils
mkdir -p ~/ct-work/vol/patches
cp -R <repo>/platforms/sdl/t32/toolchain/patches/* ~/ct-work/vol/patches/

# build
cd ~/ct-work/vol/build
cp <repo>/platforms/sdl/t32/toolchain/arm-gcc10-linux-gnueabihf.config .config
ct-ng oldconfig
ct-ng build

# install where the PolyBrik build expects it (any path works, see T32_PREFIX)
ditto ~/ct-work/vol/x-tools/arm-gcc10-linux-gnueabihf ~/x-tools/arm-gcc10-linux-gnueabihf
```

Notes from the build:

- crosstool-NG builds its own make 4.3, because glibc older than 2.32 hangs with make 4.4.
- crosstool-NG refuses a case-insensitive install prefix, so the toolchain is installed inside the case-sensitive image and then copied out. The copy to a case-insensitive disk loses one file of each of 8 Linux netfilter header pairs that differ only in case; PolyBrik does not use them.
- The toolchain is relocatable: it finds its sysroot relative to `bin/`.

## Using it

```sh
export T32_PREFIX=$HOME/x-tools/arm-gcc10-linux-gnueabihf/bin/arm-gcc10-linux-gnueabihf- T32_GLIBC=sysroot
sh platforms/sdl/t32/deps.sh build-t32   # downloads and builds static SDL 2.30.8 and alsa-lib 1.1.9 into build-t32/deps
cmake -S platforms/sdl -B build-t32/app -DCMAKE_TOOLCHAIN_FILE=$PWD/platforms/sdl/cmake/t32-armhf.cmake -DT32_DEPS=$PWD/build-t32/deps
cmake --build build-t32/app -j
sh platforms/sdl/pak/deploy-t32.sh --stage-only build-t32/app   # stages PolyBrik.pak
```

Every link prints a warning that objects use variable-size enums while the output uses 32-bit enums. It is expected: PolyBrik and Polyend's archives are built with `-fshort-enums`, glibc and libstdc++ with the Linux default. It is harmless as long as no enum type crosses between the two.
