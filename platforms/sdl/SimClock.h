#ifndef PLATFORMS_SDL_SIMCLOCK_H_
#define PLATFORMS_SDL_SIMCLOCK_H_

#include <cstdint>

// --sim-clock (golden runs): firmware time is simulated, one millisecond per main-loop
// iteration, so screenshots do not depend on how loaded the machine is. Each micros() read
// and each display frame-pacing poll also add 1 us, so a busy-wait on either still ends.
namespace SimClock
{
inline bool on = false;
inline uint64_t us = 0;
}

#endif
