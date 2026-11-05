#include "eve/Fonts.h"

#include <cstdlib>
#include <cstring>

static uint32_t le32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

FontDesc parseMetricBlock(const uint8_t* block)
{
    FontDesc f;
    memcpy(f.widths, block, 128);
    f.format = (uint8_t)le32(block + 128);
    f.stride = (uint16_t)le32(block + 132);
    f.width = (uint16_t)le32(block + 136);
    f.height = (uint16_t)le32(block + 140);
    f.pointer = le32(block + 144);
    f.valid = true;
    return f;
}

int romFontSubstitute(const FontDesc fonts[32], int romFont)
{
    static const int nominal[3] = { 19, 22, 26 };   // ROM fonts 27, 28, 29
    if (romFont < 27 || romFont > 29) return -1;
    const int target = nominal[romFont - 27];
    int best = -1, bestDiff = 0;
    for (int h = 1; h < 32; h++) {
        if (!fonts[h].valid) continue;
        const int diff = abs((int)fonts[h].height - target);
        if (best < 0 || diff < bestDiff) { best = h; bestDiff = diff; }
    }
    return best;
}
