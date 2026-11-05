#ifndef PLATFORMS_SDL_BUTTONSIMPL_H_
#define PLATFORMS_SDL_BUTTONSIMPL_H_

#include "Platform.h"
#include "core/interfaceDefs.h"

// Latched Mini key state plus press/release/hold callbacks with the
// hold curve of platforms/imx/source/Drivers/Keys.cpp:202-213.
class ButtonsImpl final : public SwitchGroup
{
  public:
    static constexpr int kButtonCount = interfaceButtonsCount; // 50 on SDL: patches 20 and 23 add the column key and the action-list keys

    void setHoldTime(int timeMs) override;
    bool isButtonPressed(int buttonIdx) override;
    void setActionCallback(ActionType action, void (*callback)(uint8_t)) override;

    // Input side. press() stamps the hold start with the last tick() time, as
    // Keys::update() stamps it with millis() in the same pass that checks holds.
    void press(uint8_t idx);
    void release(uint8_t idx);
    void tick(uint32_t nowMs);

  private:
    static constexpr int kHoldSteps = 6;
    static constexpr uint16_t kHoldStepDiff[kHoldSteps] = { 0, 100, 190, 235, 255, 270 };

    bool pressed[kButtonCount] = {};
    uint32_t holdStartMs[kButtonCount] = {};
    uint8_t holdFactor[kButtonCount] = {};
    uint16_t holdTimeMs = 333;
    uint32_t lastNowMs = 0;
    void (*onPush)(uint8_t) = nullptr;
    void (*onRelease)(uint8_t) = nullptr;
    void (*onHold)(uint8_t) = nullptr;
};

#endif
