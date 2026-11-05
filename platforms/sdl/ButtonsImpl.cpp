#include "ButtonsImpl.h"

void ButtonsImpl::setHoldTime(int timeMs) { holdTimeMs = (uint16_t)timeMs; }

bool ButtonsImpl::isButtonPressed(int buttonIdx)
{
    return buttonIdx >= 0 && buttonIdx < kButtonCount && pressed[buttonIdx];
}

void ButtonsImpl::setActionCallback(ActionType action, void (*callback)(uint8_t))
{
    switch (action)
    {
        case kPress: onPush = callback; break;
        case kRelease: onRelease = callback; break;
        case kHold: onHold = callback; break;
    }
}

void ButtonsImpl::press(uint8_t idx)
{
    if (idx >= kButtonCount || pressed[idx])
        return;
    holdStartMs[idx] = lastNowMs;
    holdFactor[idx] = 0;
    if (onPush)
        onPush(idx);
    pressed[idx] = true;
}

void ButtonsImpl::release(uint8_t idx)
{
    if (idx >= kButtonCount || !pressed[idx])
        return;
    if (onRelease)
        onRelease(idx);
    pressed[idx] = false;
}

void ButtonsImpl::tick(uint32_t nowMs)
{
    lastNowMs = nowMs;
    for (uint8_t i = 0; i < kButtonCount; i++)
    {
        if (pressed[i] &&
            (nowMs - holdStartMs[i]) > (uint32_t)(holdTimeMs - kHoldStepDiff[holdFactor[i]]))
        {
            if (holdFactor[i] < kHoldSteps - 1)
                holdFactor[i]++;
            holdStartMs[i] = nowMs;
            if (onHold)
                onHold(i);
        }
    }
}
