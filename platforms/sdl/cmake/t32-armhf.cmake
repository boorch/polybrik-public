# CMake toolchain for the T32 build (the full 32-bit app): a fully static
# armhf binary with TRACKER_MINI + PLATFORM_IMX + FEATURE_SYNTH + PLATFORM_SDL that links the
# original synth archives. Run platforms/sdl/t32/deps.sh first; it fills T32_DEPS with:
#   glibc/    (T32_GLIBC=debian, the default) Debian buster glibc 2.28 static libs and crt files
#             (kernel floor 3.2; the Homebrew crosstool sysroot's own glibc 2.28 is configured for Linux 5.10 and the Brick runs 4.9.191)
#   span/     GCC 11+ only: the toolchain's <span> with the GCC 10 member order (the archives pass
#             std::span by value); empty for a GCC 10 toolchain
#   prefix/   static alsa-lib and SDL2 (lib/, include/)
#
#   platforms/sdl/t32/deps.sh
#   cmake -S platforms/sdl -B build-t32app -DCMAKE_TOOLCHAIN_FILE=platforms/sdl/cmake/t32-armhf.cmake
#   cmake --build build-t32app -j
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# T32_PREFIX: compiler prefix including the trailing dash, a bare triple on PATH or a full path, e.g.
# ~/x-tools/arm-gcc10-linux-gnueabihf/bin/arm-gcc10-linux-gnueabihf- (env T32_PREFIX too).
if(DEFINED ENV{T32_PREFIX})
    set(_t32_prefix_default "$ENV{T32_PREFIX}")
else()
    set(_t32_prefix_default "arm-unknown-linux-gnueabihf-")
endif()
set(T32_PREFIX "${_t32_prefix_default}" CACHE STRING "cross compiler prefix, with trailing dash")
set(T32_DEPS "${CMAKE_CURRENT_LIST_DIR}/../../../build-t32app/deps" CACHE PATH "output of platforms/sdl/t32/deps.sh")
get_filename_component(T32_DEPS "${T32_DEPS}" ABSOLUTE)
set(POLYBRIK_T32 ON CACHE BOOL "full 32-bit app with the synth archives" FORCE)

set(CMAKE_C_COMPILER   "${T32_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${T32_PREFIX}g++")
# Flags. -mfp16-format=ieee: Patch::paramvalue_short_t is __fp16 in the public headers.
# -fshort-enums for every TU of ours (the archives are built with it); SDL's enum-size assert in
# SDL_stdinc.h is skipped with DOXYGEN_SHOULD_IGNORE_THIS. SDL and alsa-lib themselves are built
# int-enum by deps.sh (alsa-lib casts enum pointers to unsigned int*).
set(_t32_arch "-march=armv8-a -mthumb -mfloat-abi=hard -mfpu=fp-armv8 -mfp16-format=ieee -fshort-enums -DDOXYGEN_SHOULD_IGNORE_THIS")
set(CMAKE_C_FLAGS_INIT   "${_t32_arch}")
set(CMAKE_CXX_FLAGS_INIT "${_t32_arch} -isystem ${T32_DEPS}/span")
# -B/-L: Debian's crt1.o and libc.a win over the crosstool sysroot's (an empty dir with T32_GLIBC=sysroot).
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libstdc++ -static-libgcc -B${T32_DEPS}/glibc -L${T32_DEPS}/glibc")

set(CMAKE_FIND_ROOT_PATH "${T32_DEPS}/prefix")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
