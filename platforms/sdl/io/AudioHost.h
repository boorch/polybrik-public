#ifndef PLATFORMS_SDL_IO_AUDIOHOST_H_
#define PLATFORMS_SDL_IO_AUDIOHOST_H_

#include <cstdint>

class Tracker;
class Sgtl5000Impl;

// Renders one 128-frame block from the firmware under the audio lock and
// turns it into interleaved int16 stereo with the key mute and the codec master volume applied.
// The engine never scales the mix by the master volume itself (core/AudioEngine/mtAudioEngine.cpp
// only forwards it to codec->setMasterVolume), so the codec stub's stored value is applied here.
class AudioHost
{
  public:
    static constexpr int kBlockFrames = 128;

    // Both may be null: a null tracker renders silence, a null codec is unity gain.
    AudioHost(Tracker* tracker, const Sgtl5000Impl* codec) : tracker(tracker), codec(codec) {}

    void renderBlock(int16_t* interleavedOut); // 2 * kBlockFrames samples, LRLR

    // Gain the next block gets: 0 when muted by key, else the codec curve (1 without a codec).
    float gain() const;

    // platforms/imx/source/Sgtl5000Impl.cpp:44-69: volume 0.01..1.0 maps linearly onto the
    // SGTL5000 headphone register 0x7F..0x00 (-51.5 dB .. +12 dB, 0.5 dB per step), 0.0 mutes.
    // The firmware default (81) lands on register 24 = 0 dB, which the UI also shows as 0 dB.
    static float gainForVolume(float volume);

    // out[2i] = l[i] * gain, out[2i+1] = r[i] * gain, saturated to int16.
    static void interleave(const int16_t* l, const int16_t* r, int16_t* out, float gain);

    // Counters, written by the audio thread; read them after stop().
    uint64_t blocks() const { return blocks_; }
    uint32_t maxRenderUs() const { return maxRenderUs_; }
    uint32_t lastRenderUs() const { return lastRenderUs_; }
    uint32_t maxLockWaitUs() const { return maxLockWaitUs_; }

  private:
    Tracker* tracker;
    const Sgtl5000Impl* codec;
    uint64_t blocks_ = 0;
    uint32_t maxRenderUs_ = 0;
    uint32_t lastRenderUs_ = 0;
    uint32_t maxLockWaitUs_ = 0;
};

#endif
