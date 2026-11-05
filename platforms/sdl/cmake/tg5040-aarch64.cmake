# CMake toolchain for the TrimUI Brick / Brick Pro (NextUI "tg5040", Allwinner A133, aarch64, glibc 2.33).
# The ghcr.io/loveretro/tg5040-toolchain container needs Docker; this file instead uses as the
# compiler the Homebrew GCC cross toolchain (messense/macos-cross-toolchains, GCC 15.2, glibc 2.17
# headers and libc, older than the device's 2.33 so every libc symbol the binary needs exists there).
# SDL2 headers and the libSDL2 to link come from an aarch64 TrimUI sysroot; the device supplies
# the real /usr/trimui/lib/libSDL2-2.0.so.0 (2.30.8) at run time through launch.sh.
#
#   cmake -S platforms/sdl -B build-brick -DCMAKE_TOOLCHAIN_FILE=platforms/sdl/cmake/tg5040-aarch64.cmake
#   cmake --build build-brick -j
#
# Override the triple or the sysroot with -DTG5040_TRIPLE=... / -DTG5040_SYSROOT=... (or env TG5040_SYSROOT).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TG5040_TRIPLE "aarch64-unknown-linux-gnu" CACHE STRING "cross compiler prefix")
if(DEFINED ENV{TG5040_SYSROOT})
    set(_tg5040_sysroot_default "$ENV{TG5040_SYSROOT}")
else()
    set(_tg5040_sysroot_default "${CMAKE_CURRENT_LIST_DIR}/../../../trimui-sysroot")   # or set TG5040_SYSROOT
endif()
set(TG5040_SYSROOT "${_tg5040_sysroot_default}" CACHE PATH "aarch64 sysroot with usr/include/SDL2 and usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0")
if(NOT EXISTS "${TG5040_SYSROOT}/usr/include/SDL2/SDL.h")
    message(FATAL_ERROR "TG5040_SYSROOT ${TG5040_SYSROOT} has no usr/include/SDL2/SDL.h")
endif()

set(CMAKE_C_COMPILER   "${TG5040_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${TG5040_TRIPLE}-g++")
set(CMAKE_C_FLAGS_INIT   "-mcpu=cortex-a53")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a53")

# Only the sysroot is searched for libraries and headers; host programs stay on the host.
set(CMAKE_FIND_ROOT_PATH "${TG5040_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
