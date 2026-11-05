#include "PlatformImpl.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

PlatformImpl::SystemInfo PlatformImpl::systemInfo = { "sdl-" GIT_HASH };

PlatformImpl::PlatformImpl(const char* sdRoot, DisplayDriver* displayDriver)
  : displayDriver(displayDriver), fileSystem(sdRoot)
{
}

void PlatformImpl::init()
{
    uartMidi.init();
    fileSystem.init();
    configManager.init(&fileSystem);
}

void PlatformImpl::update()
{
    fileSystem.update();
}

// Called by the firmware at the end of its own shutdown flow. Must not return:
// the firmware loop would spin in sleep state.
void PlatformImpl::setPowerMode(bool mode)
{
    if (mode)
        return;
    displayDriver->setPowerState(false);
    fflush(nullptr);
    sync();
    exit(0);
}

// The launch script relaunches the binary on exit code 42.
void PlatformImpl::resetMCU()
{
    fflush(nullptr);
    sync();
    exit(42);
}

void PlatformImpl::runBootloader(char* fileName)
{
    fprintf(stderr, "runBootloader(%s) ignored\n", fileName ? fileName : "(null)");
}

void PlatformImpl::setPowerButtonCallback(PowerButtonCallback cb)
{
    powerButtonCallback = cb;
}

// Always false: SdlInput reports the power chord through the power button callback.
bool PlatformImpl::getPowerButtonState()
{
    return false;
}

// True skips the Mini boot power check (Tracker.cpp:47-62).
bool PlatformImpl::readForceStartUp()
{
    return true;
}
