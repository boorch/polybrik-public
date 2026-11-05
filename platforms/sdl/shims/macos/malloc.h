// macOS host shim: Darwin has no <malloc.h> and no mallinfo().
// core/source/Profiler.cpp includes "malloc.h" and calls mallinfo().fordblks.
// Only the fields core touches are provided, filled from the default malloc zone.
#ifndef PLATFORM_SDL_MACOS_MALLOC_SHIM_H_
#define PLATFORM_SDL_MACOS_MALLOC_SHIM_H_

#include <malloc/malloc.h>
#include <stdlib.h>

struct mallinfo
{
    int arena;    // bytes reserved by the zone
    int uordblks; // bytes in use
    int fordblks; // reserved but not in use
};

static inline struct mallinfo mallinfo(void)
{
    malloc_statistics_t s;
    malloc_zone_statistics(NULL, &s);
    struct mallinfo mi;
    mi.arena = (int)s.size_allocated;
    mi.uordblks = (int)s.size_in_use;
    mi.fordblks = (int)(s.size_allocated - s.size_in_use);
    return mi;
}

#endif
