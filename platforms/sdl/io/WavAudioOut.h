#ifndef PLATFORMS_SDL_IO_WAVAUDIOOUT_H_
#define PLATFORMS_SDL_IO_WAVAUDIOOUT_H_

#include "io/AudioHost.h"
#include "io/IAudioOut.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>

// 16-bit stereo 44100 Hz PCM WAV. The RIFF and data sizes are patched in close().
class WavWriter
{
  public:
    ~WavWriter() { close(); }
    bool open(const char* path);
    void write(const int16_t* interleaved, int frames);
    void close();
    uint32_t dataBytes() const { return dataBytes_; }

  private:
    void writeHeader();
    FILE* f = nullptr;
    uint32_t dataBytes_ = 0;
};

// --audio-file: a thread renders blocks in real time (deadline = start + blocks / 44100 s, no
// sleep when behind) so --keys timings and the audio line up, and writes them to a WAV.
class WavAudioOut final : public IAudioOut
{
  public:
    WavAudioOut(AudioHost& host, const char* path) : host(host), path(path) {}
    ~WavAudioOut() override { stop(); }
    bool start() override;
    void stop() override;
    void printStats(FILE* to) const override;

  private:
    void run();
    AudioHost& host;
    std::string path;
    WavWriter writer;
    std::thread thread;
    std::atomic<bool> stopFlag{false};
};

#endif
