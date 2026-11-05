// FT812 emulator: coprocessor FIFO executor,
// display-list interpreter and the memory map the firmware addresses.
// Every command list executes synchronously inside endBurst().
#pragma once

#include "eve/Canvas.h"
#include "eve/Fonts.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// Address map (FT81x.h).
// RAM_G is 2 MB, not the FT812's 1 MB: the image viewer decodes above the control snapshots
// (core displayStructs.h sdlRamGEnd, patch 30). Still below RAM_DL and inside BITMAP_SOURCE's 22 bits.
constexpr uint32_t EVE_RAM_G = 0x000000, EVE_RAM_G_SIZE = 2u << 20;
constexpr uint32_t EVE_RAM_DL = 0x300000, EVE_RAM_DL_SIZE = 8192;
constexpr uint32_t EVE_RAM_CMD = 0x308000, EVE_RAM_CMD_SIZE = 4096;

// Graphics context (FT81x defaults; VERTEX_FORMAT default is 4 = 1/16 px).
struct GfxState {
    uint32_t rgb = 0xFFFFFF;
    uint8_t alpha = 255;
    uint32_t clearRgb = 0;
    uint16_t lineWidth = 16, pointSize = 16;   // 1/16 px
    uint8_t blendSrc = BF_SRC_ALPHA, blendDst = BF_ONE_MINUS_SRC_ALPHA;
    int16_t scX = 0, scY = 0;
    uint16_t scW = 2048, scH = 2048;
    uint8_t handle = 0, cell = 0, vertexFormat = 4, prim = 0;
    Transform t { 256, 0, 0, 0, 256, 0 };
};

class EveEmu {
public:
    explicit EveEmu(Canvas& c);

    // ---- what EveDisplayDriver calls ----
    void beginBurst();                                  // burstBase = regCmdWrite; burstLen = 0
    void burst32(uint32_t w);                           // LE write at (burstBase+burstLen)&4095, byte-wise
    uint8_t burstString(const char* s, uint32_t len);   // bytes + NUL + pad to 4; returns padded count (uint8_t like upstream)
    void burstBytes(const uint8_t* p, uint32_t n);      // raw bytes, padded to 4 at the end
    void endBurst();                                    // regCmdWrite = (burstBase+burstLen)&4095; execFifo()
    uint32_t regCmdDl() const { return dlLen; }
    uint32_t regCmdRead() const { return cmdRead; }
    uint32_t regCmdWrite() const { return cmdWrite; }
    void memWrite(uint32_t addr, const uint8_t* p, uint32_t n);
    void memRead(uint32_t addr, uint8_t* p, uint32_t n);
    void fontWrite(uint32_t addr, const uint8_t* p, uint32_t n);   // bytes 144..147 := LE32(addr+148)
    void getImgProps(uint32_t& end, uint32_t& w, uint32_t& h);     // 3 LE32 at regCmdWrite-12..-4 in the ring
    bool takeFrame();                                   // true once after each CMD_SWAP
    bool collectingImage() const { return collectingJpeg; }
    std::chrono::steady_clock::time_point lastSwap() const { return lastSwapTime; }
    uint64_t lastSwapSim() const { return lastSwapSimUs; }
    // --dump-dl: each executed coprocessor command, and the displayed list length at CMD_SWAP.
    void setDumpStream(FILE* f) { dump = f; }
    const BitmapDesc& bitmapHandle(int h) const { return handles[h & 31]; }
    const FontDesc& font(int h) const { return fonts[h & 31]; }

private:
    void execFifo();
    uint32_t ring32(uint32_t off) const;
    void ringWrite32(uint32_t off, uint32_t v);
    std::string ringString(uint32_t off, uint32_t& paddedBytes) const;
    uint8_t* mem(uint32_t addr, uint32_t n);            // RAM_G / RAM_DL / RAM_CMD dispatcher, nullptr outside
    void dlAppend(uint32_t w);
    void rasterise(const uint32_t* words, size_t count, GfxState& st);
    void vertex(GfxState& st, float x, float y, int handle, int cell, int& phase, float& px, float& py);
    void expandText(int16_t x, int16_t y, uint16_t font, uint16_t opts, const std::string& s);
    void setFont2(uint32_t handle, uint32_t ptr, uint32_t firstchar);
    void setBitmap(uint32_t src, uint16_t fmt, uint16_t w, uint16_t h);
    void setMatrix();
    void loadImageFeed();
    void snapshot2(uint32_t fmt, uint32_t ptr, int x, int y, int w, int h);
    void dumpCmd(uint32_t id, uint32_t used, const char* extra);

    Canvas& canvas;
    std::vector<uint8_t> ramG, ramDl, ramCmd;
    uint32_t cmdRead = 0, cmdWrite = 0, dlLen = 0, burstBase = 0, burstLen = 0;
    BitmapDesc handles[32]{};                           // persists across DLSTART (fonts set once at boot)
    FontDesc fonts[32]{};                               // metric blocks from CMD_SETFONT2
    int romFont[3] = { -1, -1, -1 };                    // resolved substitutes for ROM fonts 27..29
    double M[6] = { 1, 0, 0, 0, 1, 0 };                 // coprocessor matrix, bitmap -> screen, [a b c; d e f]
    uint32_t numberBase = 10;
    std::vector<uint32_t> displayed;
    bool frameReady = false;
    std::chrono::steady_clock::time_point lastSwapTime{};
    uint64_t lastSwapSimUs = 0;
    // JPEG (CMD_LOADIMAGE)
    bool collectingJpeg = false;
    std::vector<uint8_t> jpeg;
    uint32_t jpegDst = 0, lastImgEnd = 0, lastImgW = 0, lastImgH = 0;
    bool logged[256] = {};                              // once per unknown command / unconfigured font
    FILE* dump = nullptr;
};
