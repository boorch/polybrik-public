#include "Sgtl5000Impl.h"

void Sgtl5000Impl::init() {}
void Sgtl5000Impl::setInputType(InputType) {}
void Sgtl5000Impl::setHeadphoneSource(HeadphoneSource) {}
void Sgtl5000Impl::setLineInLevel(int) {}
void Sgtl5000Impl::setMicGain(int) {}
void Sgtl5000Impl::setHeadphoneMuted(bool) {}
void Sgtl5000Impl::setMasterVolume(float volume) { masterVolume = volume; }
void Sgtl5000Impl::setAutoVolumeEnabled(bool) {}
void Sgtl5000Impl::configureAutoVolume(AutoVolumeParams*) {}
void Sgtl5000Impl::setEnhanceBassEnabled(bool) {}
void Sgtl5000Impl::configureEnhanceBass(EnhanceBassParams*) {}
void Sgtl5000Impl::setEqualizerType(EqualizerType) {}
void Sgtl5000Impl::configureFiveBandEqualizer(float*) {}
void Sgtl5000Impl::enableSurroundSound(bool) {}
void Sgtl5000Impl::configureSurroundSound(SurroundSoundParams*) {}
void Sgtl5000Impl::enableHardwareProcessor(bool) {}
