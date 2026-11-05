#ifndef PLATFORMS_SDL_NULLDISPLAYDRIVER_H_
#define PLATFORMS_SDL_NULLDISPLAYDRIVER_H_

#include "DisplayDriver.h"

// Headless driver: accepts every write, coprocessor always empty.
class NullDisplayDriver final : public DisplayDriver
{
  public:
    void init() {}

    void setPowerState(bool) override {}
    void setBrightness(int) override {}
    void setRotation(int) override {}
    void getImgProps(uint32_t& addr, uint32_t& width, uint32_t& height) override;
    uint32_t getDisplayListOffset() override { return 0; }
    void IncCMDOffset(uint16_t) override {}
    void WriteData32(uint32_t) override {}
    void beginCoProList() override {}
    void beginCoProListNoCheck() override {}
    void endCoProList() override {}
    void awaitCoProEmpty() override {}
    uint8_t isCoProEmpty() override { return 1; }
    void writeDataRAMG(const uint8_t*, uint32_t, uint32_t) override {}
    void writeFontRAMG(const uint8_t*, uint32_t, uint32_t) override {}
    void readDataRAMG(uint8_t* imgData, uint32_t dataSize, uint32_t) override;
    void readBackDataRAMG(uint8_t* imgData, uint32_t dataSize, uint32_t) override;
    uint8_t sendString(const char* string, uint32_t length) override;
    void writeDataToCMD(const uint8_t*, uint32_t) override {}
    void initStreamDma(uint16_t, uint16_t, uint16_t, uint8_t*, uint32_t) override {}
    bool updateStreamDma(bool) override { return true; } // same as IMX with no stream initialised
    void stopStreamDma() override {}
};

#endif
