#ifndef PLATFORMS_SDL_STUBS_BATTERYIMPL_H_
#define PLATFORMS_SDL_STUBS_BATTERYIMPL_H_

#include "Platform.h"

// isEmpty false (true powers the Mini off at boot), getState true (false shows "no battery").
class BatteryImpl final : public Battery
{
  public:
    void enableMonitoring() override;
    int getLevel() override;
    float getVoltage() override;
    bool getCharging() override;
    int getTimeTo() override;
    bool getState() override;
    bool isEmpty() override;
};

#endif
