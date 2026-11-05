#ifndef PLATFORMS_SDL_STUBS_ENCODERIMPL_H_
#define PLATFORMS_SDL_STUBS_ENCODERIMPL_H_

#include "Platform.h"

class EncoderImpl final : public Encoder
{
  public:
    void setAcceleration(int acceleration) override;
    void setResolution(int resolution) override;
};

#endif
