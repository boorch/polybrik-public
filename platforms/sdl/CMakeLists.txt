cmake_minimum_required(VERSION 3.16)
project(tracker_sdl CXX C)

# Host (Linux or macOS) and Brick build of the Tracker CE core and the SDL platform layer.
# Mirrors core/makefile's source collection and include directories, with the
# MK66/IMX specific defines removed and PLATFORM_SDL added.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)   # -std=gnu++17
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)     # -std=gnu11

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()
# We control optimisation/debug flags ourselves below.
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_DEBUG "")

set(TCE_ROOT      "${CMAKE_CURRENT_SOURCE_DIR}/../../tracker-ce")
set(CORE_DIR      "${TCE_ROOT}/core")
set(POLYUTIL_DIR  "${TCE_ROOT}/util")
set(POLYSYNTH_DIR "${TCE_ROOT}/synth")

# Source paths baked into the binary (assert messages, __FILE__) are relative to the repo root, so a
# build does not carry the builder's home directory.
get_filename_component(POLYBRIK_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_compile_options("-ffile-prefix-map=${POLYBRIK_REPO_ROOT}/=")

# ---- sources (same directories core/makefile walks with `find`) --------------
file(GLOB_RECURSE TCE_CPP_SOURCES CONFIGURE_DEPENDS
    "${CORE_DIR}/source/*.cpp"
    "${CORE_DIR}/libs/*.cpp"
    "${CORE_DIR}/interface/*.cpp"
    "${CORE_DIR}/effector/*.cpp"
    "${CORE_DIR}/FileManager/*.cpp"
    "${CORE_DIR}/Exporter/*.cpp"
    "${CORE_DIR}/AudioEngine/*.cpp"
    "${CORE_DIR}/synth/*.cpp"
)
file(GLOB_RECURSE TCE_C_SOURCES CONFIGURE_DEPENDS
    "${CORE_DIR}/libs/*.c"
)

# core/synth/** and synth_PolySynth.cpp are FEATURE_SYNTH only (IMX); the host
# build does not define FEATURE_SYNTH, so drop them. The T32 build (cmake/t32-armhf.cmake,
# the 32-bit app) is `make mini`: PLATFORM_IMX + FEATURE_SYNTH, gnu++20, the synth archives.
if(POLYBRIK_T32)
    set(CMAKE_CXX_STANDARD 20)   # -std=gnu++20: core/synth and the public synth headers need it
else()
    list(FILTER TCE_CPP_SOURCES EXCLUDE REGEX "/core/synth/")
    list(FILTER TCE_CPP_SOURCES EXCLUDE REGEX "/core/libs/Audio/synth_PolySynth\\.cpp$")
endif()

add_library(tracker_core STATIC ${TCE_CPP_SOURCES} ${TCE_C_SOURCES})

# ---- include dirs (core/makefile line 36, minus the non-existent SerialFlash/SPI)
target_include_directories(tracker_core PUBLIC
    "${CORE_DIR}/include"
    "${CORE_DIR}/config"
    "${CORE_DIR}/source"
    "${CORE_DIR}/synth"
    "${CORE_DIR}/libs"
    "${CORE_DIR}/libs/Audio"
    "${CORE_DIR}/libs/ElapsedMillis"
    "${CORE_DIR}/libs/Audio/utility"
    "${CORE_DIR}/libs/FT812"
    "${CORE_DIR}/libs/fonts"
    "${CORE_DIR}/libs/NewDisplay"
    "${CORE_DIR}/interface"
    "${CORE_DIR}/effector"
    "${CORE_DIR}/FileManager"
    "${CORE_DIR}/Exporter"
    "${CORE_DIR}/AudioEngine"
    "${CORE_DIR}/libs/Audio/Polyverb"
    "${CORE_DIR}/AudioEngine/ConstantValues"
    "${CORE_DIR}/AudioEngine/MasterEnhancer"
    "${POLYUTIL_DIR}/include"
    "${POLYUTIL_DIR}/3rdparty"
    "${POLYSYNTH_DIR}/include"
)

# ---- defines (core/makefile line 47 + version.mk, MINI product) --------------
# Dropped for host: KINETISK, __MK66FX1M0__, CPU_MK66FX1M0VMD18, F_CPU, __NEWLIB__.
# Not defined: PLATFORM_IMX, FEATURE_SYNTH, TRACKER_ARRANGEMENT, FORCED_AUTOSTART.
# FW_VER_* are MINI_VER_* from version.mk (currently equal to ORIGINAL_VER_*).
target_compile_definitions(tracker_core PUBLIC
    PLATFORM_SDL
    TRACKER_MINI
    AUDIO_PERFORMANCE_INFO_ENABLED
    MEMORY_USAGE_INFO_ENABLED
    USB_MIDI
    TEENSYDUINO=142
    ARDUINO=10805
    LAYOUT_US_ENGLISH
    FW_EDITION_NAME="CE"
    FW_VER_MAJOR=1
    FW_VER_MINOR=0
    FW_VER_PATCH=0
    FW_VER_BETA=255
)
if(POLYBRIK_T32)
    target_compile_definitions(tracker_core PUBLIC PLATFORM_IMX FEATURE_SYNTH)
endif()

# ---- flags -------------------------------------------------------------------
# -O2 on the Brick (cmake/tg5040-aarch64.cmake); -O1 -g on the host.
if(CMAKE_CROSSCOMPILING)
    set(TCE_OPT_FLAGS -O2)
else()
    set(TCE_OPT_FLAGS -O1 -g)
endif()
set(TCE_COMMON_FLAGS
    ${TCE_OPT_FLAGS} -Wall -Wno-unused-parameter
    -fno-common -ffunction-sections -fdata-sections
)
# -include cassert: on the ARM build `assert` is reached transitively through
# newlib (<stdio.h>/<string.h>/<stdlib.h>/<math.h> -> <sys/reent.h> -> <assert.h>).
# glibc does not do that, and twelve core TUs use assert() without including it.
target_compile_options(tracker_core PRIVATE
    ${TCE_COMMON_FLAGS}
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdint>"
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cassert>"
    $<$<COMPILE_LANGUAGE:CXX>:-felide-constructors>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-Werror=return-type>
)

# Host build fixes, made here instead of in core/:
# - fileManagerBrowse.cpp calls std::sort without <algorithm>; older libstdc++
#   reached it transitively, GCC 16's does not.
# - macOS has no <malloc.h>/mallinfo() (Profiler.cpp); shims/macos provides one.
set_source_files_properties("${CORE_DIR}/FileManager/fileManagerBrowse.cpp"
    PROPERTIES COMPILE_OPTIONS "-include;algorithm")
if(APPLE)
    target_include_directories(tracker_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/shims/macos")
endif()

set_target_properties(tracker_core PROPERTIES
    OUTPUT_NAME tracker_core
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ---- tracker-sdl executable ---------------------------------------------------
file(GLOB SDL_PLATFORM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/stubs/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/eve/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/io/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/io/sdl/*.cpp"
)
list(FILTER SDL_PLATFORM_SOURCES EXCLUDE REGEX "/linkcheck\\.cpp$")
if(POLYBRIK_T32)
    # T32: the io/raw framebuffer presenter replaces SdlDisplayOut; fmt's format.cc is linked as an
    # object ahead of the archives so their own newlib-built format.o (_impure_ptr, __errno) is never
    # extracted.
    file(GLOB T32_PLATFORM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/io/raw/*.cpp")
    list(APPEND SDL_PLATFORM_SOURCES ${T32_PLATFORM_SOURCES} "${POLYUTIL_DIR}/3rdparty/fmt/format.cc")
    # Static SDL2 and alsa-lib from platforms/sdl/t32/deps.sh.
    add_library(SDL2::SDL2 STATIC IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${T32_DEPS}/prefix/lib/libSDL2.a"
        INTERFACE_INCLUDE_DIRECTORIES "${T32_DEPS}/prefix/include/SDL2"
        INTERFACE_COMPILE_DEFINITIONS "_REENTRANT"
        INTERFACE_LINK_LIBRARIES "${T32_DEPS}/prefix/lib/libasound.a;m;rt;pthread;dl")   # dl: SDL_dynapi and pthread_setname_np lookup, unused unless SDL_DYNAMIC_API is set
    add_library(PolySynth STATIC IMPORTED)
    set_target_properties(PolySynth PROPERTIES IMPORTED_LOCATION "${POLYSYNTH_DIR}/lib/libPolySynth.a")
    add_library(PolyDSP STATIC IMPORTED)
    set_target_properties(PolyDSP PROPERTIES IMPORTED_LOCATION "${POLYSYNTH_DIR}/lib/libPolyDSP.a")
elseif(TG5040_SYSROOT)
    # The sysroot ships sdl2.pc but no SDL2Config.cmake: the imported target is spelled out here.
    # The device loads its own libSDL2-2.0.so.0 (same SONAME); --allow-shlib-undefined because the
    # sysroot's libSDL2 wants glibc 2.33 symbols the toolchain's 2.17 libc cannot show the linker.
    add_library(SDL2::SDL2 SHARED IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${TG5040_SYSROOT}/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0"
        # Debian's SDL_config.h includes <SDL2/_real_SDL_config.h> from the multiarch include dir.
        INTERFACE_INCLUDE_DIRECTORIES "${TG5040_SYSROOT}/usr/include/SDL2;${TG5040_SYSROOT}/usr/include;${TG5040_SYSROOT}/usr/include/aarch64-linux-gnu"
        INTERFACE_COMPILE_DEFINITIONS "_REENTRANT")
else()
    find_package(SDL2 REQUIRED)
endif()
find_package(Threads REQUIRED)
execute_process(COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE TRACKER_SDL_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
add_executable(tracker-sdl ${SDL_PLATFORM_SOURCES})
# Platform config.h must shadow core/config/config.h for platform code.
target_include_directories(tracker-sdl BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/config")
# Platform-internal includes: "eve/Canvas.h", "io/IDisplayOut.h", "third_party/stb_image.h".
target_include_directories(tracker-sdl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(tracker-sdl PRIVATE GIT_HASH="${TRACKER_SDL_GIT_HASH}")
# -fno-rtti is required: core is built without RTTI, so its interface typeinfo does not exist.
target_compile_options(tracker-sdl PRIVATE
    ${TCE_COMMON_FLAGS} -fno-rtti -fno-exceptions -Werror=return-type)
if(POLYBRIK_T32)
    # RESCAN group: the archives call back into core (registry.cpp's registerInstrumentTypes).
    # --no-warn-mismatch: the archives are tagged M-profile; -z noexecstack: they
    # carry no .note.GNU-stack.
    target_compile_definitions(tracker-sdl PRIVATE POLYBRIK_FBDEV POLYBRIK_T32)
    target_link_libraries(tracker-sdl PRIVATE "$<LINK_GROUP:RESCAN,tracker_core,PolySynth,PolyDSP>"
        SDL2::SDL2 Threads::Threads)
    target_link_options(tracker-sdl PRIVATE -Wl,--no-warn-mismatch -Wl,-z,noexecstack -Wl,--gc-sections)
else()
    target_link_libraries(tracker-sdl PRIVATE tracker_core SDL2::SDL2 Threads::Threads)
endif()
if(TG5040_SYSROOT)
    target_link_options(tracker-sdl PRIVATE -static-libstdc++ -static-libgcc -Wl,--allow-shlib-undefined)
endif()

# Tests and the link check are host diagnostics; a cross build only produces tracker-sdl.
if(NOT CMAKE_CROSSCOMPILING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

# ---- link check --------------------------------------------------------------
# linkcheck pulls the whole archive in and lets the linker report what the
# platform layer must still provide.
if(NOT CMAKE_CROSSCOMPILING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linkcheck.cpp")
    add_executable(linkcheck linkcheck.cpp)
    if(APPLE)
        # ld64 has no --whole-archive/--unresolved-symbols. -force_load pulls every
        # object; dynamic_lookup lets the link finish, and the platform contract is
        # what `nm -m linkcheck` marks "(dynamically looked up)" (libSystem and
        # libstdc++ imports are marked "(from <lib>)" instead).
        target_link_libraries(linkcheck PRIVATE
            tracker_core
            "-Wl,-force_load,$<TARGET_FILE:tracker_core>"
            -Wl,-undefined,dynamic_lookup
        )
    else()
        target_link_libraries(linkcheck PRIVATE
            -Wl,--whole-archive tracker_core -Wl,--no-whole-archive
            -Wl,--unresolved-symbols=ignore-all
        )
    endif()
    target_compile_options(linkcheck PRIVATE -fno-rtti -fno-exceptions)
endif()
