#ifndef PLATFORMS_SDL_STUBS_LEDSIMPL_H_
#define PLATFORMS_SDL_STUBS_LEDSIMPL_H_

#include "Platform.h"

class LedsImpl final : public Leds
{
  public:
    void setLed(int idx, bool enabled, int brightness) override;
};

#endif
