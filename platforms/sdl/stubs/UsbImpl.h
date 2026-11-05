#ifndef PLATFORMS_SDL_STUBS_USBIMPL_H_
#define PLATFORMS_SDL_STUBS_USBIMPL_H_

#include "Platform.h"

// Never attached; class states are only remembered (core reads them back).
class UsbImpl final : public Usb
{
  public:
    void init() override;
    void reset() override;
    void update() override;
    void setClassState(ClassType type, bool enable) override;
    bool getClassState(ClassType type) override;
    bool isAttached() override;

  private:
    bool state[5] = {};
};

#endif
