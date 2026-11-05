#ifndef PLATFORMS_SDL_STUBS_PADSIMPL_H_
#define PLATFORMS_SDL_STUBS_PADSIMPL_H_

#include "Platform.h"

// getPads() is never called by core; this only keeps the getter non-null.
class PadsImpl final : public SwitchGroup
{
  public:
    void setHoldTime(int timeMs) override;
    bool isButtonPressed(int buttonIdx) override;
    void setActionCallback(ActionType action, void (*callback)(uint8_t)) override;
};

#endif
