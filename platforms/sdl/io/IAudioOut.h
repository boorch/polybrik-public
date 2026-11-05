#ifndef PLATFORMS_SDL_IO_IAUDIOOUT_H_
#define PLATFORMS_SDL_IO_IAUDIOOUT_H_

#include <cstdio>

// Something that pulls blocks from AudioHost on its own thread: the SDL device or the WAV file.
class IAudioOut
{
  public:
    virtual ~IAudioOut() = default;
    virtual bool start() = 0;               // false: reason already printed to stderr
    virtual void stop() = 0;                // idempotent; joins the thread
    virtual void printStats(FILE* to) const = 0;
};

#endif
