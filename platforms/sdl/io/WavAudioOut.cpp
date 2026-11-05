#include "io/WavAudioOut.h"

#include <chrono>
#include <cstring>

namespace
{
constexpr uint32_t kRate = 44100;
constexpr uint16_t kChannels = 2;
constexpr uint16_t kBits = 16;

void put32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
void put16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
} // namespace

bool WavWriter::open(const char* path)
{
    close();
    f = fopen(path, "wb");
    if (!f)
    {
        perror(path);
        return false;
    }
    dataBytes_ = 0;
    writeHeader();
    return true;
}

void WavWriter::writeHeader()
{
    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    put32(h + 4, 36 + dataBytes_);
    memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16);                           // fmt chunk size
    put16(h + 20, 1);                            // PCM
    put16(h + 22, kChannels);
    put32(h + 24, kRate);
    put32(h + 28, kRate * kChannels * kBits / 8); // byte rate
    put16(h + 32, kChannels * kBits / 8);        // block align
    put16(h + 34, kBits);
    memcpy(h + 36, "data", 4);
    put32(h + 40, dataBytes_);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, sizeof(h), f);
}

void WavWriter::write(const int16_t* interleaved, int frames)
{
    if (!f)
        return;
    // shortcut: samples are written in host byte order, which is little-endian on every target
    // (arm64 macOS, the Brick); add a byte swap if a big-endian host ever appears.
    const size_t bytes = (size_t)frames * kChannels * sizeof(int16_t);
    fwrite(interleaved, 1, bytes, f);
    dataBytes_ += (uint32_t)bytes;
}

void WavWriter::close()
{
    if (!f)
        return;
    writeHeader();
    fclose(f);
    f = nullptr;
}

bool WavAudioOut::start()
{
    if (!writer.open(path.c_str()))
        return false;
    stopFlag = false;
    thread = std::thread(&WavAudioOut::run, this);
    return true;
}

void WavAudioOut::run()
{
    using Clock = std::chrono::steady_clock;
    const Clock::time_point start = Clock::now();
    int16_t block[2 * AudioHost::kBlockFrames];
    for (uint64_t n = 0; !stopFlag.load(); n++)
    {
        // Deadline of block n; sleep_until a past deadline returns at once, which is the catch-up.
        std::this_thread::sleep_until(
          start + std::chrono::nanoseconds(n * AudioHost::kBlockFrames * 1000000000ull / kRate));
        host.renderBlock(block);
        writer.write(block, AudioHost::kBlockFrames);
    }
}

void WavAudioOut::stop()
{
    stopFlag = true;
    if (thread.joinable())
        thread.join();
    writer.close();
}

void WavAudioOut::printStats(FILE* to) const
{
    fprintf(to, "audio out: wav %s, %u data bytes (%.3f s)\n", path.c_str(), writer.dataBytes(),
            writer.dataBytes() / (double)(kRate * kChannels * kBits / 8));
}
