// Free functions core links against.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

static std::recursive_mutex g_audioLock; // models "audio IRQ masked"; recursive because AudioNoInterrupts() nests

void disableAudioInterrupt() { g_audioLock.lock(); }
void enableAudioInterrupt() { g_audioLock.unlock(); }
std::recursive_mutex& audioLock() { return g_audioLock; }

static bool g_muted = false;
void muteAudioOutByKey() { g_muted = true; }
void unmuteAudioOutByKey() { g_muted = false; }
uint8_t isAudioOutMuted() { return g_muted; }

// CMSIS arm_sin_q15 semantics: 0..32767 maps to 0..2pi, result in q15.
int16_t sin_q15(int16_t x)
{
    float s = sinf((uint16_t)x * (6.2831853f / 32768.f)) * 32767.f;
    return (int16_t)std::clamp(s, -32768.f, 32767.f);
}

void _debuglog(const char* m) { fputs(m, stderr); }
void _debuglogf(const char* f, ...)
{
    va_list a;
    va_start(a, f);
    vfprintf(stderr, f, a);
    va_end(a);
}

void assert_func_play(const char* file, int line)
{
    fprintf(stderr, "assert failed: %s:%d\n", file, line);
    abort();
}

#ifdef PLATFORM_IMX
// T32 (PLATFORM_IMX): Profiler::maxStackBytes() reads the stack between these linker-script
// symbols (core/source/Profiler.cpp). Equal addresses make its scan loop return at once.
extern "C" void _vStackBase(void) {}
extern "C" void _vStackTop(void) __attribute__((alias("_vStackBase")));
#endif
