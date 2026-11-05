// synthhelper: runs the closed Polyend synth archives in a static 32-bit ARM
// Linux process, driven by the firmware's own open code: core/synth/Synth.cpp (facade, init),
// MidiAdapter (notes in), core/libs/Audio/synth_PolySynth.cpp (per-block render) on the
// Teensy AudioStream graph. No UI, no SDL, no audio device.
//
// usage: synthhelper [out.wav]   (default synthhelper.wav)

#include "AudioStream.h"
#include "Platform.h"
#include "PolySynth/Patch.h"
#include "Synth.h"
#include "mtStructs.h"
#include "synth_PolySynth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" double helper_now_ms(); // linux_syscalls.c
extern "C" double helper_cpu_ms();
extern "C" int helper_pin_cpu(int cpu);

namespace
{
constexpr int kRate = 44100;
constexpr int kBlock = AUDIO_BLOCK_SAMPLES;
constexpr double kBudgetMs = 1000.0 * kBlock / kRate; // 2.902 ms

// No SD card: the patch stores see an empty, read-only file system (newlib has no dirent, so
// the POSIX FileSystemImpl does not build here). Init patches need no files.
class NullFileSystem final : public FileSystem
{
  public:
    bool isSdCardInserted() override { return false; }
    void setMountEventCallback(CardStateChangeCallback) override {}
    bool exists(const char*) override { return false; }
    bool rename(const char*, const char*) override { return false; }
    bool mkdir(uint8_t, const char*, bool) override { return false; }
    bool remove(const char*) override { return false; }
    bool removeDirWithFiles(const char*) override { return false; }
    bool removeOnlyDirFiles(const char*) override { return false; }
    size_t getFileSize(const char*) override { return 0; }
    size_t getFileSize(FileHandle) override { return 0; }
    size_t getFileAvailable(FileHandle) override { return 0; }
    std::tuple<bool, FileHandle> openFile(const char*, FileOpenMode, FileHandle*) override
    {
        return { false, nullptr };
    }
    int32_t readFile(void*, size_t, FileHandle) override { return -1; }
    int32_t writeFile(const void*, size_t, FileHandle) override { return -1; }
    bool seekFileCur(int, FileHandle) override { return false; }
    bool seekFile(uint32_t, FileHandle) override { return false; }
    void closeFile(FileHandle, bool) override {}
    void endFile(FileHandle) override {}
    bool isFileOpen(FileHandle) override { return false; }
    bool enableFastSeek(FileHandle, uint32_t*, uint32_t, uint32_t*, uint32_t*) override { return false; }
    bool readSectorsAsync(uint8_t*, uint32_t, uint32_t) override { return false; }
    bool pollAsyncRead(bool*) override { return false; }
    bool waitAsyncRead() override { return false; }
    bool isAsyncReadBusy() override { return false; }
    int createInstrumentsList(const char*, char**, uint8_t, uint32_t*) override { return kListError; }
    int countContainedFiles(const char*) override { return 0; }
    int createFilesList(const char*, uint8_t, char**, uint8_t, uint16_t, enListFileFilter, uint16_t,
                        uint16_t, enListFileFilter) override
    {
        return kListError;
    }
    void listDirContents(const char*, DirContentType, std::function<bool(const char*)>, char*, int,
                         int, int& outNamesCount, char**) override
    {
        outNamesCount = 0;
    }
    int readNbFiles(const char*) override { return 0; }
    bool nesDir_open(const char*) override { return false; }
    bool nesDir_readItem(char*, uint8_t*, uint8_t*, uint32_t*) override { return false; }
    void nesDir_close() override {}
};

// Only getFileSystem() is used by Synth::init.
class HelperPlatform final : public Platform
{
  public:
    explicit HelperPlatform(FileSystem* fs) : fs(fs) {}
    void setPowerMode(bool) override {}
    void resetMCU() override {}
    void runBootloader(char*) override {}
    void setPowerButtonCallback(PowerButtonCallback) override {}
    bool getPowerButtonState() override { return false; }
    bool readForceStartUp() override { return false; }
    Sgtl5000* getCodecAudio() override { return nullptr; }
    Radio* getRadio() override { return nullptr; }
    SwitchGroup* getButtons() override { return nullptr; }
    SwitchGroup* getPads() override { return nullptr; }
    Leds* getLeds() override { return nullptr; }
    Battery* getBattery() override { return nullptr; }
    Timer* getTimer() override { return nullptr; }
    Encoder* getEncoder() override { return nullptr; }
    DisplayDriver* getDisplayDriver() override { return nullptr; }
    UsbAudio* getUsbAudio() override { return nullptr; }
    UartMidi* getUartMidi() override { return nullptr; }
    UsbMidi* getUsbMidi() override { return nullptr; }
    ConfigManager* getConfigManager() override { return nullptr; }
    FileSystem* getFileSystem() override { return fs; }
    CrcCalculator* getCrcCalculator() override { return nullptr; }
    SystemInfo* getSystemInfo() override { return nullptr; }
    Usb* getUsb() override { return nullptr; }

  private:
    FileSystem* fs;
};

// Stands in for the firmware's panning amplifiers plus tracks mixer: post-fader L and R of the
// three parts summed at unity (pan 0 gives gain 1 on both sides, audioEngine::setSynthPanning).
// Sends (PolySynth outputs 6..11) stay unconnected, as the default send level is 0.
class StereoSink final : public AudioStream
{
  public:
    StereoSink() : AudioStream(2 * kSynthParts, queue) {}
    void update() override
    {
        int32_t l[kBlock] = {}, r[kBlock] = {};
        for (int i = 0; i < 2 * kSynthParts; i++)
        {
            audio_block_t* b = receiveReadOnly(i);
            if (!b)
                continue;
            int32_t* dst = i < kSynthParts ? l : r;
            for (int s = 0; s < kBlock; s++)
                dst[s] += b->data[s];
            release(b);
        }
        for (int s = 0; s < kBlock; s++)
        {
            out.push_back((int16_t)std::clamp<int32_t>(l[s], INT16_MIN, INT16_MAX));
            out.push_back((int16_t)std::clamp<int32_t>(r[s], INT16_MIN, INT16_MAX));
        }
    }
    std::vector<int16_t> out; // interleaved stereo

  private:
    audio_block_t* queue[2 * kSynthParts];
};


struct Stats
{
    std::vector<double> ms;
    void print(const char* label) const
    {
        if (ms.empty())
            return;
        std::vector<double> s = ms;
        std::sort(s.begin(), s.end());
        double sum = 0;
        for (double v : s)
            sum += v;
        auto pct = [&](double q) { return s[std::min(s.size() - 1, (size_t)(q * s.size()))]; };
        size_t over = s.end() - std::upper_bound(s.begin(), s.end(), kBudgetMs);
        printf("  %-12s blocks %4zu  mean %.3f  p50 %.3f  p90 %.3f  p99 %.3f  max %.3f ms  "
               "(p99 %.0f%% of %.3f ms, %zu over)\n",
               label, s.size(), sum / s.size(), pct(0.5), pct(0.9), pct(0.99), s.back(),
               100.0 * pct(0.99) / kBudgetMs, kBudgetMs, over);
        if (getenv("SYNTHHELPER_TRACE"))
        {
            printf("    blocks over 1 ms (index:ms):");
            for (size_t i = 0; i < ms.size(); i++)
                if (ms[i] > 1.0)
                    printf(" %zu:%.2f", i, ms[i]);
            printf("\n");
        }
    }
};

Synth synth;
PolySynth polySynth; // constructed before the sink, so update_all renders it first
StereoSink sink;

// One firmware audio block: update_all() runs PolySynth::update then the sink. Timed twice:
// wall clock (what an audio deadline sees) and thread CPU time (the render cost alone).
void renderBlocks(int count, Stats* wall, Stats* cpu)
{
    for (int i = 0; i < count; i++)
    {
        double w0 = helper_now_ms(), c0 = helper_cpu_ms();
        AudioStream::update_all();
        double c1 = helper_cpu_ms(), w1 = helper_now_ms();
        if (wall)
            wall->ms.push_back(w1 - w0);
        if (cpu)
            cpu->ms.push_back(c1 - c0);
    }
}

int blocksFor(double seconds) { return (int)(seconds * kRate / kBlock); }

// Settings into mtProject, then Synth::updateToSettings (project load). Then what the instrument
// editor does after a type change (SynthEditor.cpp initCurrentPatch): macro defaults from the new
// patch and engine.refreshSynthInstrumentParams, which the audio engine applies as
// setSynthGain (gain dB) and setSynthMacro (CC 102 + macro on channel part + 1). Pan 0 and
// sends 0 are the sink's unity sum and the unconnected send outputs.
void configure(int synthTypeIdx, const int voices[kSynthParts])
{
    constexpr int kMacroCCIdxStart = 102; // SynthVoice.h
    for (int p = 0; p < kSynthParts; p++)
    {
        mtProject.values.synthPart[p].synthType = synthTypeIdx;
        mtProject.values.synthPart[p].voiceCount = voices[p];
    }
    synth.updateToSettings(mtProject.values, true);
    for (int p = 0; p < kSynthParts; p++)
    {
        auto& settings = mtProject.values.synthPart[p];
        synth.initSynthPartMacrosValues(p, settings);
        polySynth.setGain(p, settings.gainDb);
        for (int m = 0; m < kSynthMacrosCount; m++)
            synth.getVirtualMidiPort()->sendControlChange(kMacroCCIdxStart + m, settings.macroValue[m], p + 1);
    }
}

void writeWav(const char* path, const std::vector<int16_t>& pcm)
{
    FILE* f = fopen(path, "wb");
    if (!f)
    {
        perror(path);
        return;
    }
    uint32_t dataBytes = pcm.size() * 2, riffSize = 36 + dataBytes, fmtSize = 16, rate = kRate,
             byteRate = kRate * 4;
    uint16_t pcmFmt = 1, ch = 2, align = 4, bits = 16;
    fwrite("RIFF", 1, 4, f);
    fwrite(&riffSize, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmtSize, 4, 1, f);
    fwrite(&pcmFmt, 2, 1, f);
    fwrite(&ch, 2, 1, f);
    fwrite(&rate, 4, 1, f);
    fwrite(&byteRate, 4, 1, f);
    fwrite(&align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataBytes, 4, 1, f);
    fwrite(pcm.data(), 2, pcm.size(), f);
    fclose(f);
}
} // namespace

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (const char* cpu = getenv("SYNTHHELPER_CPU")) // no taskset on the device
        printf("pinned to cpu %s: %s\n", cpu, helper_pin_cpu(atoi(cpu)) == 0 ? "ok" : "failed");
    const char* wavPath = argc > 1 ? argv[1] : "synthhelper.wav";

    AudioMemory(64);

    NullFileSystem fs;
    HelperPlatform platform(&fs);

    // Tracker.cpp:125 then SynthVoice::init (engine.init).
    synth.init(&platform, []() { return tiny_optional<float>(130.0f); });
    polySynth.init(&synth);

    // Wiring of audioEngine::connectSynthToMixer, post-fader outputs only.
    std::vector<AudioConnection*> cords;
    for (int p = 0; p < kSynthParts; p++)
    {
        cords.push_back(new AudioConnection(&polySynth, p, &sink, p));
        cords.push_back(new AudioConnection(&polySynth, PolySynth::kRightChannelOffset + p, &sink,
                                            kSynthParts + p));
    }

    ::Midi* midi = synth.getVirtualMidiPort();
    printf("synthhelper: %d Hz, %d-frame blocks, budget %.3f ms per block\n", kRate, kBlock, kBudgetMs);

    // 1. Pitch check: SYN1 alone, type 0, one voice, A4 (MIDI 69) held 2 s; expect about 440 Hz
    // (the ACD init patch decays in about 0.3 s, so measure the start).
    {
        const int voices[kSynthParts] = { 1, 0, 0 };
        configure(0, voices);
        printf("pitch: SYN1 %s, note 69 at samples 0..%d\n", synth.getSynthName(0).c_str(),
               blocksFor(2.0) * kBlock);
        midi->sendNoteOn(69, 100, 1);
        renderBlocks(blocksFor(2.0), nullptr, nullptr);
        midi->sendNoteOff(69, 0, 1);
        renderBlocks(blocksFor(0.5), nullptr, nullptr);
    }

    // 2. Every engine on all three parts: 3 + 3 + 2 voices, a chord per part, held 2 s.
    const int chord[kSynthParts][3] = { { 48, 52, 55 }, { 60, 64, 67 }, { 72, 76, 79 } };
    const int voices[kSynthParts] = { 3, 3, 2 };
    Stats all, allCpu;
    for (int t = 0; t < (int)Synth::kSynthTypes.size(); t++)
    {
        configure(t, voices);
        int sounding = 0;
        for (int p = 0; p < kSynthParts; p++)
            sounding += std::min(voices[p], synth.getPatch(p).getMetadata().maxVoices);
        printf("engine %d %-14s start sample %zu, voices requested 3+3+2, patch maxVoices %d -> %d sounding\n",
               t, synth.getInstrumentTypeName(t).c_str(), sink.out.size() / 2,
               synth.getPatch(0).getMetadata().maxVoices, sounding);
        for (int p = 0; p < kSynthParts; p++)
            for (int n = 0; n < voices[p]; n++)
                midi->sendNoteOn(chord[p][n], 100, p + 1);
        Stats st, stCpu;
        renderBlocks(blocksFor(2.0), &st, &stCpu);
        for (int p = 0; p < kSynthParts; p++)
            for (int n = 0; n < voices[p]; n++)
                midi->sendNoteOff(chord[p][n], 0, p + 1);
        renderBlocks(blocksFor(0.75), nullptr, nullptr);
        st.print("wall");
        stCpu.print("cpu");
        all.ms.insert(all.ms.end(), st.ms.begin(), st.ms.end());
        allCpu.ms.insert(allCpu.ms.end(), stCpu.ms.begin(), stCpu.ms.end());
    }
    printf("render time, 3 parts sounding (all engines):\n");
    all.print("wall");
    allCpu.print("cpu");

    int16_t peak = 0;
    for (int16_t s : sink.out)
        peak = std::max<int16_t>(peak, std::abs(s));
    printf("wav: %s, %zu frames, peak %d (%.1f dBFS), audio blocks in use max %d\n", wavPath,
           sink.out.size() / 2, peak, 20 * std::log10(std::max(1, (int)peak) / 32768.0),
           AudioStream::memory_used_max);
    writeWav(wavPath, sink.out);
    return peak > 0 ? 0 : 2;
}
