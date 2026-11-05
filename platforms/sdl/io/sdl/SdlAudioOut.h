#ifndef PLATFORMS_SDL_IO_SDL_SDLAUDIOOUT_H_
#define PLATFORMS_SDL_IO_SDL_SDLAUDIOOUT_H_

#include "io/AudioHost.h"
#include "io/IAudioOut.h"

#include <SDL.h>
#include <cstdint>

// Default SDL output device, 44100 Hz S16 stereo, fed from AudioHost in whole 128-frame blocks.
class SdlAudioOut final : public IAudioOut
{
  public:
    explicit SdlAudioOut(AudioHost& host) : host(host) {}
    ~SdlAudioOut() override { stop(); }
    bool start() override;
    void stop() override;
    void printStats(FILE* to) const override;

    const SDL_AudioSpec& obtained() const { return obtained_; }
    uint32_t callbacks() const { return callbacks_; }
    // SDL never reports an underrun; a callback arriving later than 1.5x the buffer period is counted.
    // Measured on macOS 2.32 CoreAudio: 128-frame callbacks come in bursts of four even with an
    // empty callback, so there this counts bursts (about a quarter of all callbacks), not dropouts.
    uint32_t lateCallbacks() const { return lateCallbacks_; }

    // Fills `frames` stereo frames from whole host blocks; the unused tail of the last block is
    // carried over to the next call, so any length works.
    void fillFrames(int16_t* out, int frames);

  private:
    static void callback(void* user, Uint8* stream, int len);

    AudioHost& host;
    SDL_AudioDeviceID dev = 0;
    SDL_AudioSpec obtained_{};
    int16_t carry[2 * AudioHost::kBlockFrames];
    int carryPos = 2 * AudioHost::kBlockFrames; // samples already consumed; full = empty
    uint32_t callbacks_ = 0;
    uint32_t lateCallbacks_ = 0;
    int64_t lastCallbackUs = 0;
    int64_t expectedPeriodUs = 0;
};

#endif
