// Link check: pull all of libtracker_core.a in and let the linker report
// which symbols the platform layer must still provide.
#include "Tracker.h"

#include <cstdio>

int main()
{
    Tracker* t = Tracker::getInstance();
    std::printf("%p %s\n", (void*)t, Tracker::getVersion());
    return 0;
}
