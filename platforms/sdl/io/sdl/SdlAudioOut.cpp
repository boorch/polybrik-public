#include "io/sdl/SdlAudioOut.h"

#include <chrono>
#include <cstring>

bool SdlAudioOut::start()
{
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_STREAM_NAME, "tracker-sdl");
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
#ifdef POLYBRIK_T32
    // SDL's ALSA backend asks for 2 periods of `samples`. The synth archives have render spikes of
    // 5 to 8 ms (one WTFM block about 1.1 s after note-on, measured with synthhelper), so the device
    // gets 512-frame periods: 4 blocks (11.6 ms) are still queued when a callback starts.
    want.samples = 4 * AudioHost::kBlockFrames;
#else
    want.samples = AudioHost::kBlockFrames;
#endif
    want.callback = callback;
    want.userdata = this;
    // Only the buffer size may differ; SDL converts rate, format and channels itself otherwise.
    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &obtained_, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (dev == 0)
    {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return false;
    }
    expectedPeriodUs = (int64_t)obtained_.samples * 1000000 / obtained_.freq;
    SDL_PauseAudioDevice(dev, 0);
    return true;
}

void SdlAudioOut::stop()
{
    if (dev == 0)
        return;
    SDL_CloseAudioDevice(dev); // waits for a running callback
    dev = 0;
}

void SdlAudioOut::fillFrames(int16_t* out, int frames)
{
    constexpr int kCarrySamples = 2 * AudioHost::kBlockFrames;
    while (frames > 0)
    {
        if (carryPos == kCarrySamples)
        {
            host.renderBlock(carry);
            carryPos = 0;
        }
        const int avail = (kCarrySamples - carryPos) / 2;
        const int n = frames < avail ? frames : avail;
        memcpy(out, carry + carryPos, (size_t)n * 2 * sizeof(int16_t));
        out += 2 * n;
        carryPos += 2 * n;
        frames -= n;
    }
}

void SdlAudioOut::callback(void* user, Uint8* stream, int len)
{
    SdlAudioOut* self = static_cast<SdlAudioOut*>(user);
    const int64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    if (self->lastCallbackUs != 0 && now - self->lastCallbackUs > self->expectedPeriodUs * 3 / 2)
        self->lateCallbacks_++;
    self->lastCallbackUs = now;
    self->callbacks_++;
    self->fillFrames(reinterpret_cast<int16_t*>(stream), len / (int)(2 * sizeof(int16_t)));
}

void SdlAudioOut::printStats(FILE* to) const
{
    fprintf(to, "audio out: sdl device, %u callbacks, %u late callbacks (gap > 1.5x period; bursts count on CoreAudio), obtained %d Hz fmt 0x%x %d ch %u frames (%u bytes)\n",
            callbacks_, lateCallbacks_, obtained_.freq, obtained_.format, obtained_.channels,
            obtained_.samples, obtained_.size);
}
