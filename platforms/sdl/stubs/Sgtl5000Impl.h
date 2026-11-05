#ifndef PLATFORMS_SDL_STUBS_SGTL5000IMPL_H_
#define PLATFORMS_SDL_STUBS_SGTL5000IMPL_H_

#include "Platform.h"

// Codec stub. setMasterVolume is kept for the audio thread to apply as software gain.
class Sgtl5000Impl final : public Sgtl5000
{
  public:
    void init() override;
    void setInputType(InputType input) override;
    void setHeadphoneSource(HeadphoneSource source) override;
    void setLineInLevel(int level) override;
    void setMicGain(int gainDb) override;
    void setHeadphoneMuted(bool muted) override;
    void setMasterVolume(float volume) override;
    void setAutoVolumeEnabled(bool enabled) override;
    void configureAutoVolume(AutoVolumeParams* params) override;
    void setEnhanceBassEnabled(bool enabled) override;
    void configureEnhanceBass(EnhanceBassParams* params) override;
    void setEqualizerType(EqualizerType type) override;
    void configureFiveBandEqualizer(float* values) override;
    void enableSurroundSound(bool enabled) override;
    void configureSurroundSound(SurroundSoundParams* params) override;
    void enableHardwareProcessor(bool enabled) override;

    float getMasterVolume() const { return masterVolume; }

  private:
    float masterVolume = 1.0f;
};

#endif
