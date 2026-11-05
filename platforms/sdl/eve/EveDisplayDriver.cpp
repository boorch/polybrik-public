#include "eve/EveDisplayDriver.h"
#include "SimClock.h"
#include "displayStructs.h"

static_assert(EVE_RAM_G_SIZE == sdlRamGEnd, "core image addresses assume the emulator's RAM_G size");

// 0 until 16 ms after the last CMD_SWAP; always 1 while a LOADIMAGE collects bytes.
uint8_t EveDisplayDriver::isCoProEmpty()
{
    if (emu.collectingImage()) return 1;
    // processDisplayUpdateImmediate busy-waits on this, so each poll lets 1 simulated us pass.
    if (SimClock::on) return ++SimClock::us - emu.lastSwapSim() >= 16000u ? 1 : 0;
    return std::chrono::steady_clock::now() - emu.lastSwap() >= std::chrono::milliseconds(16) ? 1 : 0;
}

// API_LIB_WriteDataToCMD (API_Layer.cpp:302): JPEG bytes appended to the FIFO in bursts of at
// most 1000 bytes, padded to 4 at the end. The firmware passes 5000-byte chunks (display.cpp:434),
// more than the 4 KB ring holds, so each burst is executed (collected) before the next is written.
// Upstream off-by-one kept: a 1-byte call sends nothing (EndIndex 0 skips the loop).
void EveDisplayDriver::writeDataToCMD(const uint8_t* imgData, uint32_t totalDataSize)
{
    if (totalDataSize < 2) return;
    for (uint32_t off = 0; off < totalDataSize; off += 1000) {
        const uint32_t n = totalDataSize - off < 1000 ? totalDataSize - off : 1000;
        emu.beginBurst();
        emu.burstBytes(imgData + off, n);
        emu.endBurst();
    }
}
