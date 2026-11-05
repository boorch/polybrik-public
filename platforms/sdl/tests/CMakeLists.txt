# Self-checks for the SDL platform layer. Each test lists its own sources so a
# half-written file in another folder cannot break it. Run the binary; it exits
# non-zero on failure. Catch2 is vendored at tracker-ce/util/3rdparty/catch2 if a
# test wants it; assert-based mains (fs_test) are fine too.
set(PSDL ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(TEST_FLAGS -O0 -g -Wall -Wno-unused-parameter)

# --- fs lane ---
add_executable(fs_test fs_test.cpp ${PSDL}/FileSystemImpl.cpp)
target_include_directories(fs_test PRIVATE ${PSDL})
target_link_libraries(fs_test PRIVATE tracker_core)
target_compile_options(fs_test PRIVATE ${TEST_FLAGS})

# --- canvas lane (eve/Canvas) ---
add_executable(canvas_test canvas_test.cpp ${PSDL}/eve/Canvas.cpp)
target_include_directories(canvas_test PRIVATE ${PSDL})
target_compile_options(canvas_test PRIVATE ${TEST_FLAGS})

# --- eve lane (eve/EveEmu, eve/Fonts, eve/EveDisplayDriver) ---
# tracker_core gives FT81x.h, DisplayDriver.h and the firmware's bitmaps[] table.
add_executable(eve_test eve_test.cpp
    ${PSDL}/eve/EveEmu.cpp ${PSDL}/eve/Fonts.cpp ${PSDL}/eve/EveDisplayDriver.cpp ${PSDL}/eve/Canvas.cpp)
target_include_directories(eve_test PRIVATE ${PSDL})
target_link_libraries(eve_test PRIVATE tracker_core)
target_compile_options(eve_test PRIVATE ${TEST_FLAGS})

# --- out lane (io/, io/sdl/SdlDisplayOut, png) ---
add_executable(png_test png_test.cpp ${PSDL}/io/Png.cpp ${PSDL}/eve/Canvas.cpp)
target_include_directories(png_test PRIVATE ${PSDL})
target_compile_options(png_test PRIVATE ${TEST_FLAGS})

# --- input lane (io/sdl/SdlInput, InputMap) ---
# SdlInput takes a real PlatformImpl (power callback), so its member Impls are listed;
# tracker_core gives the include dirs, defines and the two PolyUtil string helpers
# FileSystemImpl/ConfigManagerImpl call. SDL2 for the event queue and scancode names.
add_executable(input_test input_test.cpp
    ${PSDL}/InputMap.cpp ${PSDL}/io/sdl/SdlInput.cpp ${PSDL}/ButtonsImpl.cpp
    ${PSDL}/PlatformImpl.cpp ${PSDL}/ConfigManagerImpl.cpp ${PSDL}/FileSystemImpl.cpp
    ${PSDL}/NullDisplayDriver.cpp ${PSDL}/TimerImpl.cpp
    ${PSDL}/stubs/BatteryImpl.cpp ${PSDL}/stubs/CrcCalculatorImpl.cpp
    ${PSDL}/stubs/EncoderImpl.cpp ${PSDL}/stubs/LedsImpl.cpp ${PSDL}/stubs/PadsImpl.cpp
    ${PSDL}/stubs/RadioImpl.cpp ${PSDL}/stubs/Sgtl5000Impl.cpp ${PSDL}/stubs/UartMidiImpl.cpp
    ${PSDL}/stubs/UsbAudioImpl.cpp ${PSDL}/stubs/UsbImpl.cpp ${PSDL}/stubs/UsbMidiImpl.cpp)
target_include_directories(input_test BEFORE PRIVATE ${PSDL}/config) # same shadow as tracker-sdl
target_include_directories(input_test PRIVATE ${PSDL})
target_link_libraries(input_test PRIVATE tracker_core SDL2::SDL2)
target_compile_options(input_test PRIVATE ${TEST_FLAGS})

# --- audio lane (io/AudioHost, io/WavAudioOut, io/sdl/SdlAudioOut) ---
# glue.cpp gives the audio lock and the key mute; SDL2 for SdlAudioOut's class (no device is opened).
add_executable(audio_test audio_test.cpp
    ${PSDL}/io/AudioHost.cpp ${PSDL}/io/WavAudioOut.cpp ${PSDL}/io/sdl/SdlAudioOut.cpp
    ${PSDL}/glue.cpp ${PSDL}/stubs/Sgtl5000Impl.cpp)
target_include_directories(audio_test PRIVATE ${PSDL})
target_link_libraries(audio_test PRIVATE tracker_core SDL2::SDL2 Threads::Threads)
target_compile_options(audio_test PRIVATE ${TEST_FLAGS})

# --- legend lane (io/Legend, the strip under the canvas) ---
# tracker_core gives FT81x.h, the Roboto 12 px font data and decompress(); the test
# defines kModuleNamesList itself so the module objects are not pulled in. SDL2 only for
# the include path (InputMap.h, kLayerInput).
add_executable(legend_test legend_test.cpp
    ${PSDL}/io/Legend.cpp ${PSDL}/io/Png.cpp
    ${PSDL}/eve/EveEmu.cpp ${PSDL}/eve/Fonts.cpp ${PSDL}/eve/EveDisplayDriver.cpp ${PSDL}/eve/Canvas.cpp)
target_include_directories(legend_test PRIVATE ${PSDL})
target_link_libraries(legend_test PRIVATE tracker_core SDL2::SDL2)
target_compile_options(legend_test PRIVATE ${TEST_FLAGS})

# --- action list lane (core/interface/core/sdlActionList, core patch 24) ---
# Headless state machine only: initDisplayControls() is never called. The display object
# drags interface.cpp and the audio engine in, so glue.cpp gives the audio lock, key mute
# and sin_q15 (as for audio_test).
add_executable(actionlist_test actionlist_test.cpp ${PSDL}/NullDisplayDriver.cpp ${PSDL}/glue.cpp ${PSDL}/ButtonsImpl.cpp)
target_include_directories(actionlist_test PRIVATE ${PSDL})
target_link_libraries(actionlist_test PRIVATE tracker_core Threads::Threads)
target_compile_options(actionlist_test PRIVATE ${TEST_FLAGS})
