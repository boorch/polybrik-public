#ifndef PLATFORMS_SDL_STUBS_USBAUDIOIMPL_H_
#define PLATFORMS_SDL_STUBS_USBAUDIOIMPL_H_

#include "Platform.h"

class UsbAudioImpl final : public UsbAudio
{
  public:
    void beginFillData() override;
    void fillChannelData(void* samples, int count, int channel) override;
    void finishFillData() override;
};

#endif
