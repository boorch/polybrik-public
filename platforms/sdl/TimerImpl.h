#ifndef PLATFORMS_SDL_TIMERIMPL_H_
#define PLATFORMS_SDL_TIMERIMPL_H_

#include "Platform.h"

// Construct after SDL_Init.
class TimerImpl final : public Timer
{
  public:
    TimerImpl();
    uint32_t millis() override;
    uint32_t micros() override;
    void delay(int ms) override;
    uint32_t clockCycles() override;
    uint32_t clockCyclesPerUs() override;

  private:
    uint64_t t0;
    uint64_t pc0;
};

#endif
