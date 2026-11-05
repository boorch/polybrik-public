#include "io/AudioHost.h"

#include "Tracker.h"
#include "stubs/Sgtl5000Impl.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>

// glue.cpp
std::recursive_mutex& audioLock();
uint8_t isAudioOutMuted();

namespace
{
using Clock = std::chrono::steady_clock;

uint32_t usSince(Clock::time_point t)
{
    return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t).count();
}
} // namespace

float AudioHost::gainForVolume(float volume)
{
    if (volume <= 0.0f)
        return 0.0f;
    const float v = volume < 0.01f ? 0.01f : (volume > 1.0f ? 1.0f : volume);
    const uint8_t reg = (uint8_t)(127.0f * (1.0f - v) / 0.99f); // mapClamped(v, 0.01, 1.0, 0x7F, 0x00)
    return powf(10.0f, (12.0f - 0.5f * reg) / 20.0f);
}

float AudioHost::gain() const
{
    if (isAudioOutMuted())
        return 0.0f;
    return codec ? gainForVolume(codec->getMasterVolume()) : 1.0f;
}

void AudioHost::interleave(const int16_t* l, const int16_t* r, int16_t* out, float gain)
{
    for (int i = 0; i < kBlockFrames; i++)
    {
        float a = l[i] * gain;
        float b = r[i] * gain;
        out[2 * i] = (int16_t)(a > 32767.0f ? 32767.0f : (a < -32768.0f ? -32768.0f : a));
        out[2 * i + 1] = (int16_t)(b > 32767.0f ? 32767.0f : (b < -32768.0f ? -32768.0f : b));
    }
}

void AudioHost::renderBlock(int16_t* interleavedOut)
{
    // The output stream keeps these pointers (core/libs/Audio/Output.cpp:49-53) and the exporter
    // writes through them from the main thread in render mode (Output.cpp:8-29 via artificial_isr,
    // mtAudioExport.cpp:713), so like the DMA buffers on hardware they must outlive this call.
    static int16_t left[kBlockFrames];
    static int16_t right[kBlockFrames];
    const Clock::time_point t0 = Clock::now();
    {
        std::lock_guard<std::recursive_mutex> guard(audioLock());
        const Clock::time_point t1 = Clock::now();
        const uint32_t wait = usSince(t0);
        if (wait > maxLockWaitUs_)
            maxLockWaitUs_ = wait;
        if (tracker)
        {
            // Same two calls per block as platforms/imx/source/PlatformTrackerIMX.cpp:80-102.
            tracker->updateInputAudioBuffers(nullptr, nullptr, kBlockFrames);
            tracker->updateAudioBuffers(left, right, kBlockFrames);
        }
        else
        {
            memset(left, 0, sizeof(left));
            memset(right, 0, sizeof(right));
        }
        lastRenderUs_ = usSince(t1);
        if (lastRenderUs_ > maxRenderUs_)
            maxRenderUs_ = lastRenderUs_;
    }
    blocks_++;
    interleave(left, right, interleavedOut, gain());
}
