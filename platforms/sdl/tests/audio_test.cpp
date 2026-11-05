// Assert-based self-check of the audio lane: WavWriter, AudioHost interleave/gain/mute,
// SdlAudioOut carry buffer, and a null-tracker render. Needs no audio device.
#undef NDEBUG
#include "Tracker.h"
#include "io/AudioHost.h"
#include "io/WavAudioOut.h"
#include "io/sdl/SdlAudioOut.h"
#include "stubs/Sgtl5000Impl.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

// glue.cpp
void muteAudioOutByKey();
void unmuteAudioOutByKey();

namespace
{
constexpr int kFrames = AudioHost::kBlockFrames;

// Emits a running counter so continuity across blocks and carry boundaries is checkable.
class FakeTracker final : public Tracker
{
  public:
    void init(Platform*) override {}
    void update() override {}
    void updateAudioBuffers(void* left, void* right, int length) override
    {
        assert(length == kFrames);
        int16_t* l = static_cast<int16_t*>(left);
        int16_t* r = static_cast<int16_t*>(right);
        for (int i = 0; i < length; i++, counter++)
        {
            l[i] = counter;
            r[i] = (int16_t)-counter;
        }
        blocks++;
    }
    void updateInputAudioBuffers(void* i2s, void* usb, int length) override
    {
        assert(i2s == nullptr && usb == nullptr && length == kFrames);
        inputCalls++;
    }
    void (*getStartDisplayFunc())(Platform*) override { return nullptr; }
    Platform* getPlatform() override { return nullptr; }

    int16_t counter = 0;
    int blocks = 0;
    int inputCalls = 0;
};

uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
int16_t rds16(const uint8_t* p) { return (int16_t)rd16(p); }

void testWavWriter()
{
    char tmpl[] = "/tmp/audio_test_XXXXXX";
    assert(mkdtemp(tmpl));
    const std::string path = std::string(tmpl) + "/out.wav";

    const int kBlocks = 1000;
    {
        WavWriter w;
        assert(w.open(path.c_str()));
        int16_t block[2 * kFrames];
        for (int b = 0; b < kBlocks; b++)
        {
            for (int i = 0; i < kFrames; i++)
            {
                block[2 * i] = (int16_t)(b * kFrames + i);       // L: frame index
                block[2 * i + 1] = (int16_t)-(b * kFrames + i);  // R: negated
            }
            w.write(block, kFrames);
        }
        assert(w.dataBytes() == (uint32_t)kBlocks * kFrames * 4);
        w.close();
    }

    FILE* f = fopen(path.c_str(), "rb");
    assert(f);
    std::vector<uint8_t> file;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        file.insert(file.end(), buf, buf + n);
    fclose(f);

    const uint32_t dataBytes = (uint32_t)kBlocks * kFrames * 4;
    assert(file.size() == 44 + dataBytes);
    assert(!memcmp(&file[0], "RIFF", 4));
    assert(rd32(&file[4]) == 36 + dataBytes);
    assert(!memcmp(&file[8], "WAVEfmt ", 8));
    assert(rd32(&file[16]) == 16);
    assert(rd16(&file[20]) == 1);        // PCM
    assert(rd16(&file[22]) == 2);        // channels
    assert(rd32(&file[24]) == 44100);    // rate
    assert(rd32(&file[28]) == 176400);   // byte rate
    assert(rd16(&file[32]) == 4);        // block align
    assert(rd16(&file[34]) == 16);       // bits
    assert(!memcmp(&file[36], "data", 4));
    assert(rd32(&file[40]) == dataBytes);
    assert(rds16(&file[44]) == 0 && rds16(&file[46]) == 0);              // first frame
    const int16_t last = (int16_t)(kBlocks * kFrames - 1);
    assert(rds16(&file[44 + dataBytes - 4]) == last);                     // last L
    assert(rds16(&file[44 + dataBytes - 2]) == (int16_t)-last);           // last R
    printf("wav writer: %zu bytes, header and first/last samples ok (%s)\n", file.size(), path.c_str());
}

void testInterleaveAndGain()
{
    int16_t l[kFrames], r[kFrames], out[2 * kFrames];
    for (int i = 0; i < kFrames; i++)
    {
        l[i] = (int16_t)(i * 100);
        r[i] = (int16_t)(-i * 100);
    }
    AudioHost::interleave(l, r, out, 1.0f);
    for (int i = 0; i < kFrames; i++)
        assert(out[2 * i] == l[i] && out[2 * i + 1] == r[i]);
    AudioHost::interleave(l, r, out, 0.0f);
    for (int i = 0; i < 2 * kFrames; i++)
        assert(out[i] == 0);
    AudioHost::interleave(l, r, out, 0.5f);
    for (int i = 0; i < kFrames; i++)
        assert(out[2 * i] == i * 50 && out[2 * i + 1] == -i * 50);
    AudioHost::interleave(l, r, out, 100.0f); // saturates
    assert(out[2 * 127] == 32767 && out[2 * 127 + 1] == -32768 && out[0] == 0);

    // Codec curve (platforms/imx Sgtl5000Impl mapping): 0 mutes, default 81 is unity, 100 is +12 dB.
    assert(AudioHost::gainForVolume(0.0f) == 0.0f);
    assert(fabsf(AudioHost::gainForVolume(0.81f) - 1.0f) < 1e-5f);
    assert(fabsf(AudioHost::gainForVolume(1.0f) - powf(10.0f, 12.0f / 20.0f)) < 1e-4f);
    assert(fabsf(AudioHost::gainForVolume(0.01f) - powf(10.0f, -51.5f / 20.0f)) < 1e-6f);
    assert(AudioHost::gainForVolume(0.5f) < AudioHost::gainForVolume(0.6f));
    printf("interleave and gain curve ok\n");
}

void testMuteAndCodec()
{
    Sgtl5000Impl codec;
    AudioHost host(nullptr, &codec);
    // The stub's initial masterVolume is 1.0f (before the engine sets the config value), the +12 dB top.
    assert(fabsf(host.gain() - AudioHost::gainForVolume(1.0f)) < 1e-6f);
    codec.setMasterVolume(0.81f);
    assert(fabsf(host.gain() - 1.0f) < 1e-5f);
    muteAudioOutByKey();
    assert(host.gain() == 0.0f);
    unmuteAudioOutByKey();
    assert(fabsf(host.gain() - 1.0f) < 1e-5f);
    codec.setMasterVolume(0.0f);
    assert(host.gain() == 0.0f);

    AudioHost bare(nullptr, nullptr);
    assert(bare.gain() == 1.0f);
    muteAudioOutByKey();
    assert(bare.gain() == 0.0f);
    unmuteAudioOutByKey();

    // Mute reaches the rendered block: a fake tracker emits a counter, muted output is all zero.
    FakeTracker fake;
    AudioHost live(&fake, nullptr);
    int16_t out[2 * kFrames];
    live.renderBlock(out);
    assert(out[2] == 1 && out[3] == -1);
    muteAudioOutByKey();
    live.renderBlock(out);
    for (int i = 0; i < 2 * kFrames; i++)
        assert(out[i] == 0);
    unmuteAudioOutByKey();
    live.renderBlock(out);
    assert(out[0] == 256 && out[1] == -256); // the muted block still advanced the sequencer
    assert(fake.blocks == 3 && fake.inputCalls == 3 && live.blocks() == 3);
    printf("mute and codec volume ok\n");
}

void testCarry()
{
    FakeTracker fake;
    AudioHost host(&fake, nullptr);
    SdlAudioOut out(host); // never started: no device needed for fillFrames
    std::vector<int16_t> stream;
    const int lengths[] = {100, 100, 56, 300, 84, 1, 127, 512, 3};
    int total = 0;
    for (int len : lengths)
    {
        std::vector<int16_t> chunk((size_t)len * 2);
        out.fillFrames(chunk.data(), len);
        stream.insert(stream.end(), chunk.begin(), chunk.end());
        total += len;
        // whole blocks only: rendered frames never exceed requested by a full block
        assert((int)host.blocks() * kFrames >= total && (int)host.blocks() * kFrames - total < kFrames);
    }
    assert(total == 1283 && host.blocks() == 11);
    for (int k = 0; k < total; k++)
        assert(stream[(size_t)2 * k] == k && stream[(size_t)2 * k + 1] == -k);
    printf("carry buffer: %d frames over %d calls, %llu blocks, continuity ok\n", total,
           (int)(sizeof(lengths) / sizeof(lengths[0])), (unsigned long long)host.blocks());
}

void testNullTracker()
{
    AudioHost host(nullptr, nullptr);
    int16_t out[2 * kFrames];
    for (int b = 0; b < 100; b++)
    {
        memset(out, 0x55, sizeof(out));
        host.renderBlock(out);
        for (int i = 0; i < 2 * kFrames; i++)
            assert(out[i] == 0);
    }
    assert(host.blocks() == 100);
    printf("null tracker: 100 blocks of silence, max render %u us, max lock wait %u us\n",
           host.maxRenderUs(), host.maxLockWaitUs());
}
// Same shape as main.cpp's power-off path: a WavAudioOut thread is running when exit() is called,
// and an atexit handler registered in main stops it and patches the header. The file is checked
// from the shell (RIFF size must not be the empty 36).
AudioHost* g_atexitHost = nullptr;
WavAudioOut* g_atexitOut = nullptr;
void stopAtexit()
{
    g_atexitOut->stop();
    printf("atexit: wav stopped after %llu blocks\n", (unsigned long long)g_atexitHost->blocks());
    fflush(stdout);
}
} // namespace

int main()
{
    testWavWriter();
    testInterleaveAndGain();
    testMuteAndCodec();
    testCarry();
    testNullTracker();

    static AudioHost atexitHost(nullptr, nullptr);
    const char* path = "/tmp/audio_test_atexit.wav";
    static WavAudioOut atexitOut(atexitHost, path);
    assert(atexitOut.start());
    g_atexitHost = &atexitHost;
    g_atexitOut = &atexitOut;
    atexit(stopAtexit);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printf("audio_test: all ok, exiting with a live wav thread to %s\n", path);
    exit(0);
}
