// Assert-based self-check of EveEmu, Fonts and EveDisplayDriver.
// Catch2 is vendored as the v3 source tree (dozens of .cpp files), so this is an
// assert-based main like fs_test.cpp. Every expectation below is hand-computed.
#undef NDEBUG
#include "eve/Canvas.h"
#include "eve/EveDisplayDriver.h"
#include "eve/EveEmu.h"
#include "eve/Fonts.h"

#include "FT81x.h"
#include "displayStructs.h"   // strBitmap (tracker_core include path)
// bitmaps[] lives in display.o, which drags the whole firmware and the platform
// contract into the link, so the logo comes from its own header with the table
// entry of display.cpp:141 copied verbatim.
#include "images/poly_logo_172x172_L8.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

// 16x16 solid white baseline JPEG (773 bytes, sips quality 90), used by the large-chunk test.
static const uint8_t kWhite16Jpg[] = {
  0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
  0x01, 0x00, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xe1, 0x00, 0x4c,
  0x45, 0x78, 0x69, 0x66, 0x00, 0x00, 0x4d, 0x4d, 0x00, 0x2a, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x01, 0x87, 0x69, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xa0, 0x01,
  0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xa0, 0x02,
  0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10, 0xa0, 0x03,
  0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x00, 0xff, 0xed, 0x00, 0x38, 0x50, 0x68, 0x6f, 0x74, 0x6f, 0x73,
  0x68, 0x6f, 0x70, 0x20, 0x33, 0x2e, 0x30, 0x00, 0x38, 0x42, 0x49, 0x4d,
  0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x42, 0x49, 0x4d,
  0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xd4, 0x1d, 0x8c, 0xd9,
  0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e,
  0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x10, 0x03, 0x01, 0x22,
  0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00, 0x1f, 0x00,
  0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0xff, 0xc4, 0x00, 0xb5, 0x10, 0x00, 0x02, 0x01,
  0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01,
  0x7d, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41,
  0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1,
  0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62,
  0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44,
  0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74,
  0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
  0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2,
  0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5,
  0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
  0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3,
  0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xc4, 0x00, 0x1f, 0x01,
  0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0xff, 0xc4, 0x00, 0xb5, 0x11, 0x00, 0x02, 0x01,
  0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02,
  0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12,
  0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42,
  0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72,
  0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a,
  0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43,
  0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57,
  0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73,
  0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86,
  0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
  0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3,
  0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
  0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
  0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3,
  0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xdb, 0x00, 0x43, 0x00,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x02, 0x03, 0x02,
  0x02, 0x02, 0x03, 0x04, 0x03, 0x03, 0x03, 0x03, 0x04, 0x05, 0x04, 0x04,
  0x04, 0x04, 0x04, 0x05, 0x06, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
  0x09, 0x09, 0x09, 0x09, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x01, 0x01, 0x01,
  0x02, 0x02, 0x02, 0x04, 0x02, 0x02, 0x04, 0x09, 0x06, 0x05, 0x06, 0x09,
  0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
  0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
  0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
  0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
  0x09, 0xff, 0xdd, 0x00, 0x04, 0x00, 0x01, 0xff, 0xda, 0x00, 0x0c, 0x03,
  0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00, 0xfe, 0xfe, 0x28,
  0xa2, 0x8a, 0x00, 0xff, 0xd9
};

static uint32_t le32(const uint8_t* p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t dlWord(EveEmu& emu, int index)
{
    uint8_t b[4];
    emu.memRead(EVE_RAM_DL + 4 * index, b, 4);
    return le32(b);
}

// Synthetic metric block: 'A' 10 px, 'B' 12 px, 'F' 9 px, everything else 7 px; L4 glyphs
// 17x20 (stride 9); the pointer field is patched by writeFontRAMG.
static std::vector<uint8_t> makeFont()
{
    std::vector<uint8_t> f(148 + 128 * 9 * 20, 0xFF);
    for (int i = 0; i < 128; i++) f[i] = 7;
    f['A'] = 10; f['B'] = 12; f['F'] = 9;
    const uint32_t fields[5] = { 2, 9, 17, 20, 0 };
    for (int i = 0; i < 5; i++) memcpy(&f[128 + 4 * i], &fields[i], 4);
    return f;
}

static void uploadFont(EveDisplayDriver& drv, int handle, uint32_t addr)
{
    std::vector<uint8_t> f = makeFont();
    drv.writeFontRAMG(f.data(), (uint32_t)f.size(), addr);
    drv.beginCoProList();
    drv.WriteData32(CMD_SETFONT2); drv.WriteData32(handle); drv.WriteData32(addr); drv.WriteData32(0);
    drv.endCoProList();
}

static void cmdText(EveDisplayDriver& drv, int16_t x, int16_t y, uint16_t font, uint16_t opts, const char* s)
{
    drv.WriteData32(CMD_TEXT);
    drv.WriteData32(((uint32_t)(uint16_t)y << 16) | (uint16_t)x);
    drv.WriteData32(((uint32_t)opts << 16) | font);
    drv.sendString(s, 0);
}

static void frameHeader(EveDisplayDriver& drv)
{
    drv.WriteData32(CMD_DLSTART);
    drv.WriteData32(CLEAR_COLOR_RGB(0, 0, 0));
    drv.WriteData32(CLEAR(1, 1, 1));
    drv.WriteData32(VERTEX_FORMAT(0));
}

int main()
{
    // ---- metric block parse --------------------------------------------------------
    {
        std::vector<uint8_t> f = makeFont();
        const uint32_t ptr = 0x12345678;
        memcpy(&f[144], &ptr, 4);
        FontDesc d = parseMetricBlock(f.data());
        assert(d.valid && d.widths['A'] == 10 && d.widths['B'] == 12 && d.widths[0] == 7);
        assert(d.format == 2 && d.stride == 9 && d.width == 17 && d.height == 20 && d.pointer == 0x12345678);
    }

    // ---- writeFontRAMG pointer patch (bytes 144..147 := dst + 148) -----------------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        std::vector<uint8_t> f = makeFont();
        f[150] = 0x5A;
        drv.writeFontRAMG(f.data(), 156, 1000);
        uint8_t out[156];
        emu.memRead(1000, out, 156);
        assert(memcmp(out, f.data(), 144) == 0);
        assert(le32(out + 144) == 1148);
        assert(out[150] == 0x5A && memcmp(out + 148, f.data() + 148, 8) == 0);
    }

    // ---- sendString padding and return value ------------------------------------------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        drv.beginCoProList();
        assert(drv.sendString("ab", 0) == 4);        // 2 + NUL = 3 -> 4
        assert(drv.sendString("abcd", 0) == 8);      // 4 + NUL = 5 -> 8
        assert(drv.sendString("abcdef", 3) == 4);    // clamped to 3 + NUL = 4
        assert(drv.sendString("ab", 10) == 4);       // clamped to strlen
        uint8_t ring[20];
        emu.memRead(EVE_RAM_CMD, ring, 20);
        assert(memcmp(ring, "ab\0\0abcd\0\0\0\0abc\0ab\0\0", 20) == 0);
        drv.endCoProList();
        assert(emu.regCmdWrite() == 20 && emu.regCmdRead() == 20);
    }

    // ---- FIFO ring wrap: a burst across the 4096 boundary --------------------------------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        uploadFont(drv, 1, 1000);            // 16 bytes in the FIFO
        drv.beginCoProList();
        drv.WriteData32(CMD_DLSTART);
        for (int i = 0; i < 1016; i++) drv.WriteData32(COLOR_A(255));
        drv.endCoProList();                  // 16 + 4 + 4064 = 4084
        assert(emu.regCmdWrite() == 4084 && emu.regCmdDl() == 4064);
        drv.beginCoProList();
        cmdText(drv, 10, 10, 1, 0, "Hello");   // 12 + 8 bytes: 4084..4095 then 0..7
        drv.endCoProList();
        assert(emu.regCmdWrite() == 8 && emu.regCmdRead() == 8);
        assert(emu.regCmdDl() == 4064 + 10 * 4);
        const int last = 4064 / 4 + 10 - 1;
        assert(dlWord(emu, last) == RESTORE_CONTEXT());
        assert(dlWord(emu, last - 1) == END());
        assert(dlWord(emu, last - 2) == VERTEX2II(38, 10, 1, 'o'));   // 10 + 4 * 7
        assert(dlWord(emu, last - 9) == SAVE_CONTEXT());
    }

    // ---- REG_CMD_DL exact byte count: DLSTART, 3 DL words, 5-char TEXT, APPEND ---------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        uploadFont(drv, 1, 1000);
        uint32_t snippet[6];
        for (int i = 0; i < 6; i++) snippet[i] = COLOR_RGB(i, i, i);
        drv.writeDataRAMG((const uint8_t*)snippet, 24, 150000);
        drv.beginCoProListNoCheck();
        frameHeader(drv);                                  // DLSTART + 3 words = 12
        cmdText(drv, 0, 0, 1, 0, "Hello");                  // 5 + 5 words = 40
        drv.WriteData32(CMD_APPEND); drv.WriteData32(150000); drv.WriteData32(24);   // 24
        drv.endCoProList();
        assert(drv.getDisplayListOffset() == 12 + 40 + 24);
        assert(dlWord(emu, 13) == COLOR_RGB(0, 0, 0) && dlWord(emu, 18) == COLOR_RGB(5, 5, 5));
        // CMD_MEMCPY from RAM_DL to a control slot (step2), then APPEND of the copy.
        drv.beginCoProListNoCheck();
        drv.WriteData32(CMD_MEMCPY); drv.WriteData32(150000 + 8192); drv.WriteData32(RAM_DL); drv.WriteData32(76);
        drv.endCoProList();
        uint8_t copy[76], orig[76];
        emu.memRead(150000 + 8192, copy, 76);
        emu.memRead(EVE_RAM_DL, orig, 76);
        assert(memcmp(copy, orig, 76) == 0);
    }

    // ---- CMD_TEXT option maths -----------------------------------------------------------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        uploadFont(drv, 1, 1000);
        struct Case { uint16_t opts; int x0, x1, y; } cases[] = {
            { OPT_CENTERX, 89, 99, 50 },     // total 22, x -= 11
            { OPT_RIGHTX, 78, 88, 50 },      // x -= 22
            { OPT_CENTERY, 100, 110, 40 },   // y -= 20 / 2
            { OPT_CENTER, 89, 99, 40 },
        };
        for (const Case& c : cases) {
            drv.beginCoProList();
            drv.WriteData32(CMD_DLSTART);
            cmdText(drv, 100, 50, 1, c.opts, "AB");
            drv.endCoProList();
            assert(emu.regCmdDl() == 7 * 4);
            assert(dlWord(emu, 0) == SAVE_CONTEXT() && dlWord(emu, 1) == BITMAP_HANDLE(1) && dlWord(emu, 2) == BEGIN(BITMAPS));
            assert(dlWord(emu, 3) == VERTEX2II(c.x0, c.y, 1, 'A'));
            assert(dlWord(emu, 4) == VERTEX2II(c.x1, c.y, 1, 'B'));
            assert(dlWord(emu, 5) == END() && dlWord(emu, 6) == RESTORE_CONTEXT());
        }
        // Beyond 511: CELL + VERTEX2F in whole pixels.
        drv.beginCoProList();
        drv.WriteData32(CMD_DLSTART);
        cmdText(drv, 600, 50, 1, 0, "A");
        drv.endCoProList();
        assert(emu.regCmdDl() == 7 * 4);
        assert(dlWord(emu, 3) == CELL('A') && dlWord(emu, 4) == VERTEX2F(600, 50));
    }

    // ---- rotated-text matrix (label.cpp:209-213, font height 17) ---------------------------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        drv.beginCoProList();
        drv.WriteData32(CMD_DLSTART);
        drv.WriteData32(CMD_LOADIDENTITY);
        drv.WriteData32(CMD_TRANSLATE); drv.WriteData32(65536 * (17 / 2)); drv.WriteData32(65536 * (17 / 2));
        drv.WriteData32(CMD_ROTATE); drv.WriteData32(270 * 65536 / 360);
        drv.WriteData32(CMD_TRANSLATE); drv.WriteData32((uint32_t)(65536 * (-17 / 2))); drv.WriteData32((uint32_t)(65536 * (-17 / 2)));
        drv.WriteData32(CMD_SETMATRIX);
        drv.endCoProList();
        // inverse(T(8,8) R(270 cw) T(-8,-8)) = [0 -1 16; 1 0 0]
        assert(emu.regCmdDl() == 24);
        assert(dlWord(emu, 0) == 0x15000000u);   // A = 0
        assert(dlWord(emu, 1) == 0x1601FF00u);   // B = -1.0 (8.8, 17-bit two's complement)
        assert(dlWord(emu, 2) == 0x17001000u);   // C = 16.0 (15.8)
        assert(dlWord(emu, 3) == 0x18000100u);   // D = 1.0
        assert(dlWord(emu, 4) == 0x19000000u);   // E = 0
        assert(dlWord(emu, 5) == 0x1A000000u);   // F = 0
        // The matrix persists until LOADIDENTITY: a second SETMATRIX gives the same words.
        drv.beginCoProList();
        drv.WriteData32(CMD_DLSTART);
        drv.WriteData32(CMD_SETMATRIX);
        drv.endCoProList();
        assert(dlWord(emu, 1) == 0x1601FF00u && dlWord(emu, 2) == 0x17001000u);
    }

    // ---- scissor reset at DLSTART, handle persistence across DLSTART, takeFrame ---------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        const uint8_t white[16] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
        drv.writeDataRAMG(white, 16, 2000);
        assert(drv.isCoProEmpty() == 1);
        assert(!emu.takeFrame());
        drv.beginCoProList();
        frameHeader(drv);
        drv.WriteData32(SCISSOR_XY(0, 0)); drv.WriteData32(SCISSOR_SIZE(10, 10));
        drv.WriteData32(COLOR_RGB(255, 0, 0));
        drv.WriteData32(BEGIN(RECTS)); drv.WriteData32(VERTEX2F(0, 0)); drv.WriteData32(VERTEX2F(100, 100)); drv.WriteData32(END());
        drv.WriteData32(BITMAP_HANDLE(7)); drv.WriteData32(BITMAP_SOURCE(2000));
        drv.WriteData32(BITMAP_LAYOUT(L8, 4, 4)); drv.WriteData32(BITMAP_SIZE(NEAREST, BORDER, BORDER, 4, 4));
        drv.WriteData32(FT_DISPLAY()); drv.WriteData32(CMD_SWAP);
        drv.endCoProList();
        assert(drv.isCoProEmpty() == 0);   // paced: 16 ms after the swap
        assert(emu.takeFrame() && !emu.takeFrame());
        const uint32_t* px = canvas.pixels();
        assert(px[5 * 800 + 5] == 0xFFFF0000u);
        assert(px[50 * 800 + 50] == 0xFF000000u);   // scissored
        drv.beginCoProList();
        frameHeader(drv);
        drv.WriteData32(COLOR_RGB(0, 255, 0));
        drv.WriteData32(BEGIN(RECTS)); drv.WriteData32(VERTEX2F(0, 0)); drv.WriteData32(VERTEX2F(100, 100)); drv.WriteData32(END());
        drv.WriteData32(BITMAP_HANDLE(7)); drv.WriteData32(BEGIN(BITMAPS)); drv.WriteData32(VERTEX2F(200, 200)); drv.WriteData32(END());
        drv.WriteData32(FT_DISPLAY()); drv.WriteData32(CMD_SWAP);
        drv.endCoProList();
        assert(px[50 * 800 + 50] == 0xFF00FF00u);     // scissor reset to full screen
        assert(px[201 * 800 + 201] == 0xFF00FF00u);   // handle 7 still configured from frame 1
        assert(px[205 * 800 + 205] == 0xFF000000u);
        assert(emu.bitmapHandle(7).source == 2000 && emu.bitmapHandle(7).format == L8 && emu.bitmapHandle(7).width == 4);
    }

    // ---- CMD_SETBASE persistence and CMD_NUMBER --------------------------------------------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        uploadFont(drv, 1, 1000);
        drv.beginCoProList();
        drv.WriteData32(CMD_SETBASE); drv.WriteData32(16);
        drv.endCoProList();
        drv.beginCoProList();
        drv.WriteData32(CMD_DLSTART);
        drv.WriteData32(CMD_NUMBER); drv.WriteData32(0); drv.WriteData32(1); drv.WriteData32(255);
        drv.endCoProList();
        assert(emu.regCmdDl() == 7 * 4);
        assert(dlWord(emu, 3) == VERTEX2II(0, 0, 1, 'F') && dlWord(emu, 4) == VERTEX2II(9, 0, 1, 'F'));
        drv.beginCoProList();
        drv.WriteData32(CMD_SETBASE); drv.WriteData32(10);
        drv.WriteData32(CMD_DLSTART);
        drv.WriteData32(CMD_NUMBER); drv.WriteData32(0); drv.WriteData32(1); drv.WriteData32(255);
        drv.endCoProList();
        assert(emu.regCmdDl() == 8 * 4);
        assert(dlWord(emu, 3) == VERTEX2II(0, 0, 1, '2') && dlWord(emu, 5) == VERTEX2II(14, 0, 1, '5'));
    }

    // ---- ROM font 27..29 substitution picks the nearest uploaded height ----------------------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        uploadFont(drv, 3, 1000);   // height 20
        drv.beginCoProList();
        drv.WriteData32(CMD_DLSTART);
        cmdText(drv, 0, 0, 29, 0, "A");
        drv.endCoProList();
        assert(dlWord(emu, 1) == BITMAP_HANDLE(3) && dlWord(emu, 3) == VERTEX2II(0, 0, 3, 'A'));
    }

    // ---- CMD_LOADIMAGE collection, writeDataToCMD, GETPROPS slots ---------------------------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        drv.beginCoProListNoCheck();
        drv.WriteData32(CMD_LOADIMAGE); drv.WriteData32(350000); drv.WriteData32(OPT_NODL);
        drv.endCoProList();
        assert(emu.collectingImage() && drv.isCoProEmpty() == 1);
        const uint8_t one = 0xFF;
        drv.writeDataToCMD(&one, 1);           // upstream off-by-one: nothing sent
        assert(emu.regCmdWrite() == 12);
        const uint8_t bad[5] = { 0xFF, 0xD8, 0x00, 0xFF, 0xD9 };   // not a decodable JPEG
        drv.writeDataToCMD(bad, 3);
        assert(emu.collectingImage());
        drv.writeDataToCMD(bad + 3, 2);
        assert(!emu.collectingImage() && emu.regCmdWrite() == 20 && emu.regCmdRead() == 20);
        drv.beginCoProList();
        drv.WriteData32(CMD_GETPROPS); drv.WriteData32(0); drv.WriteData32(0); drv.WriteData32(0);
        drv.endCoProList();
        uint32_t end = 1, w = 1, h = 1;
        drv.getImgProps(end, w, h);
        assert(end == 350000 && w == 0 && h == 0);
    }

    // ---- writeDataToCMD chunks larger than the 4 KB ring (display.cpp:434 passes 5000) ----------
    // A COM segment (FF FE, length 6002) of 6000 zero bytes is spliced in after SOI; stb_image skips
    // it, so the padded file is still the 16x16 white image, 773 + 4 + 6000 = 6777 bytes.
    {
        std::vector<uint8_t> big = { 0xFF, 0xD8, 0xFF, 0xFE, 0x17, 0x72 };   // 0x1772 = 6002
        big.resize(big.size() + 6000, 0);
        big.insert(big.end(), kWhite16Jpg + 2, kWhite16Jpg + sizeof kWhite16Jpg);
        assert(big.size() == 6777);
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        drv.beginCoProListNoCheck();
        drv.WriteData32(CMD_LOADIMAGE); drv.WriteData32(0x1000); drv.WriteData32(OPT_NODL);
        drv.endCoProList();
        drv.writeDataToCMD(big.data(), 5000);           // as display.cpp does: imgBufforSize first,
        assert(emu.collectingImage() && drv.isCoProEmpty() == 1);
        drv.writeDataToCMD(big.data() + 5000, 1777);    // then the eof remainder
        assert(!emu.collectingImage());
        // ring: 12 + 5000 + 1777 padded to 1780 = 6792, mod 4096 = 2696, fully consumed
        assert(emu.regCmdWrite() == 2696 && emu.regCmdRead() == 2696);
        drv.beginCoProList();
        drv.WriteData32(CMD_GETPROPS); drv.WriteData32(0); drv.WriteData32(0); drv.WriteData32(0);
        drv.endCoProList();
        uint32_t end = 0, w = 0, h = 0;
        drv.getImgProps(end, w, h);
        assert(w == 16 && h == 16 && end == 0x1000 + 16 * 16 * 2);
        uint8_t px[2];
        emu.memRead(0x1000 + 2 * (16 * 15 + 15), px, 2);   // last pixel, RGB565 white
        assert(px[0] == 0xFF && px[1] == 0xFF);
    }

    // ---- boot logo smoke test: display.cpp:begin lines 319-347 -------------------------------
    {
        Canvas canvas(800, 480);
        EveEmu emu(canvas);
        EveDisplayDriver drv(emu);
        const strBitmap bm = { poly_logo_172x172_l8, 80000, 29584, 172, 172, L8, 172 };   // display.cpp:151-159
        drv.writeDataRAMG(bm.data, bm.source, bm.address);
        drv.beginCoProList();
        frameHeader(drv);
        drv.WriteData32(SAVE_CONTEXT());
        drv.WriteData32(BITMAP_HANDLE(0));
        drv.WriteData32(BITMAP_SOURCE(bm.address));
        drv.WriteData32(BITMAP_LAYOUT(bm.format, bm.linestride, bm.height));
        drv.WriteData32(BITMAP_SIZE(NEAREST, BORDER, BORDER, bm.width, bm.height));
        drv.WriteData32(BEGIN(BITMAPS));
        drv.WriteData32(VERTEX2F((800 / 2) - (bm.width / 2), 100));
        drv.WriteData32(END());
        drv.WriteData32(RESTORE_CONTEXT());
        drv.WriteData32(FT_DISPLAY());
        drv.WriteData32(CMD_SWAP);
        drv.endCoProList();
        const int x0 = 400 - bm.width / 2, y0 = 100;
        int inside = 0, outside = 0;
        const uint32_t* px = canvas.pixels();
        for (int y = 0; y < 480; y++)
            for (int x = 0; x < 800; x++) {
                const bool in = x >= x0 && x < x0 + bm.width && y >= y0 && y < y0 + bm.height;
                if (px[y * 800 + x] != 0xFF000000u) (in ? inside : outside)++;
            }
        printf("logo %ux%u at (%d,%d): %d lit pixels inside, %d outside\n", bm.width, bm.height, x0, y0, inside, outside);
        assert(inside > 0 && outside == 0);
    }

    puts("eve_test: all checks passed");
    return 0;
}
