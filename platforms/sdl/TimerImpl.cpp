#include "TimerImpl.h"
#include "SimClock.h"

#include <SDL.h>

TimerImpl::TimerImpl() : t0(SDL_GetTicks64()), pc0(SDL_GetPerformanceCounter()) {}

uint32_t TimerImpl::millis()
{
    if (SimClock::on) return (uint32_t)(SimClock::us / 1000u);
    return (uint32_t)(SDL_GetTicks64() - t0);
}

uint32_t TimerImpl::micros()
{
    if (SimClock::on) return (uint32_t)(++SimClock::us);
    uint64_t d = SDL_GetPerformanceCounter() - pc0;
    uint64_t f = SDL_GetPerformanceFrequency();
    return (uint32_t)(d / f * 1000000u + (d % f) * 1000000u / f); // split to avoid 64-bit overflow
}

void TimerImpl::delay(int ms)
{
    if (SimClock::on)
    {
        SimClock::us += (uint64_t)(ms > 0 ? ms : 0) * 1000u;
        return;
    }
    if (ms > 2)
    {
        SDL_Delay(ms);
        return;
    }
    uint32_t start = micros();
    while (micros() - start < (uint32_t)ms * 1000u) {}
}

uint32_t TimerImpl::clockCycles() { return micros() * 1000u; }
uint32_t TimerImpl::clockCyclesPerUs() { return 1000; }
