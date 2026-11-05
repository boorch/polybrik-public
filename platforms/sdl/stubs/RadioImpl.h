#ifndef PLATFORMS_SDL_STUBS_RADIOIMPL_H_
#define PLATFORMS_SDL_STUBS_RADIOIMPL_H_

#include "Platform.h"

// Tracker Mini has no radio: bodies from platforms/imx/source/RadioImpl.cpp (TRACKER_MINI),
// except getStationName, which returns a valid string because mtTest calls it unguarded.
class RadioImpl final : public Radio
{
  public:
    void enableLineInput(bool) override;
    void enable(bool) override;
    bool setFrequency(float) override;
    void setSeekCallback(void (*)(void)) override;
    void seek(SeekDirection) override;
    void updateSeek() override;
    bool updateRDS() override;
    float getFrequency() override;
    float getBaseFrequency() override;
    float getSpacing() override;
    bool isinitialized() override;
    void clearRDS() override;
    void setRegion(Region) override;
    bool isRegionChanged() override;
    char* getStationName() override;
};

#endif
