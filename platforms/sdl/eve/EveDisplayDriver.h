// DisplayDriver on top of EveEmu. Every command list
// executes inside endCoProList(); isCoProEmpty() is paced to 16 ms per swap.
#pragma once

#include "DisplayDriver.h"
#include "eve/EveEmu.h"

class EveDisplayDriver final : public DisplayDriver
{
  public:
    explicit EveDisplayDriver(EveEmu& emu) : emu(emu) {}
    void init() {}

    void setPowerState(bool state) override { power = state; }
    void setBrightness(int value) override { brightnessValue = value; }
    void setRotation(int) override {}
    // For the presenter (IDisplayOut) to poll; 0..128 as cDisplay passes it.
    int brightness() const { return brightnessValue; }
    bool powerState() const { return power; }

    void getImgProps(uint32_t& addr, uint32_t& width, uint32_t& height) override { emu.getImgProps(addr, width, height); }
    uint32_t getDisplayListOffset() override { return emu.regCmdDl(); }
    void IncCMDOffset(uint16_t) override {}   // bookkeeping only; the emulator trusts the burst length
    void WriteData32(uint32_t data) override { emu.burst32(data); }

    void beginCoProList() override { emu.beginBurst(); }
    void beginCoProListNoCheck() override { emu.beginBurst(); }
    void endCoProList() override { emu.endBurst(); }
    void awaitCoProEmpty() override {}
    uint8_t isCoProEmpty() override;
    void writeDataRAMG(const uint8_t* imgData, uint32_t dataSize, uint32_t destAddress) override { emu.memWrite(destAddress, imgData, dataSize); }
    void writeFontRAMG(const uint8_t* imgData, uint32_t dataSize, uint32_t destAddress) override { emu.fontWrite(destAddress, imgData, dataSize); }
    void readDataRAMG(uint8_t* imgData, uint32_t dataSize, uint32_t sourceAddress) override { emu.memRead(sourceAddress, imgData, dataSize); }
    void readBackDataRAMG(uint8_t* imgData, uint32_t dataSize, uint32_t sourceAddress) override { emu.memRead(sourceAddress, imgData, dataSize); }
    uint8_t sendString(const char* string, uint32_t length) override { return emu.burstString(string, length); }
    void writeDataToCMD(const uint8_t* imgData, uint32_t totalDataSize) override;

    // Dead video path, stubbed as DisplayDriverImpl.cpp:138-218 behaves with no stream.
    void initStreamDma(uint16_t, uint16_t, uint16_t, uint8_t*, uint32_t) override { streamInitialized = true; }
    bool updateStreamDma(bool lastFrame) override { return !streamInitialized; }
    void stopStreamDma() override { streamInitialized = false; }

  private:
    EveEmu& emu;
    int brightnessValue = 128;
    bool power = true;
    bool streamInitialized = false;
};
