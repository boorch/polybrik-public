// FT81x font metric blocks: the 148-byte header the firmware uploads
// with writeFontRAMG and registers with CMD_SETFONT2.
#pragma once

#include <cstdint>

struct FontDesc {
    uint8_t widths[128] = {};   // @0: advance per character code
    uint8_t format = 0;         // @128
    uint16_t stride = 0;        // @132
    uint16_t width = 0;         // @136
    uint16_t height = 0;        // @140
    uint32_t pointer = 0;       // @144: address of the first glyph (cell 0 of the uploaded set)
    bool valid = false;
};

// Parses the 148-byte block at `block` (little-endian 32-bit fields).
FontDesc parseMetricBlock(const uint8_t* block);

// ROM fonts 27, 28, 29 have no glyph data in the emulator.
// shortcut: ROM fonts 27-29 reuse the nearest uploaded Liberation handle, replace with
// build-time glyph sets if the no-SD or battery screens look wrong.
// Returns the valid handle in fonts[1..31] whose height is nearest to the ROM font's
// nominal size (19, 22, 26 px), or -1 when no font has been uploaded yet.
int romFontSubstitute(const FontDesc fonts[32], int romFont);
