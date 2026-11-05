#include "RadioImpl.h"

void RadioImpl::enableLineInput(bool) {}
void RadioImpl::enable(bool) {}
bool RadioImpl::setFrequency(float) { return false; }
void RadioImpl::setSeekCallback(void (*)(void)) {}
void RadioImpl::seek(SeekDirection) {}
void RadioImpl::updateSeek() {}
bool RadioImpl::updateRDS() { return false; }
float RadioImpl::getFrequency() { return 0.0f; }
float RadioImpl::getBaseFrequency() { return 0.0f; }
float RadioImpl::getSpacing() { return 0.0f; }
bool RadioImpl::isinitialized() { return false; }
void RadioImpl::clearRDS() {}
void RadioImpl::setRegion(Region) {}
bool RadioImpl::isRegionChanged() { return false; }
char* RadioImpl::getStationName()
{
    static char name[] = "";
    return name;
}
