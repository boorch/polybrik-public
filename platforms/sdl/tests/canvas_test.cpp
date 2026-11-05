// Assert-based self-check of eve/Canvas: hand-computed pixels, no screenshots.
#undef NDEBUG
#include "eve/Canvas.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static const Clip FULL = {0, 0, 800, 480};
static Paint paint(uint32_t rgb, uint8_t a = 255, uint8_t srcF = BF_SRC_ALPHA, uint8_t dstF = BF_ONE_MINUS_SRC_ALPHA, Clip clip = FULL)
{
    return Paint{rgb, a, srcF, dstF, clip};
}
static uint32_t px(const Canvas& c, int x, int y) { return c.pixels()[y * c.width() + x]; }
static int countNot(const Canvas& c, uint32_t bg)
{
    int n = 0;
    for (int i = 0; i < c.width() * c.height(); i++) n += c.pixels()[i] != bg;
    return n;
}

static const uint32_t BG = 0xFF000000u, W = 0xFFFFFFFFu;

int main()
{
    Canvas c(800, 480);
    assert(c.width() == 800 && c.height() == 480);
    assert(px(c, 0, 0) == BG);

    // disc r=1 at (10,10): only the four centres at distance sqrt(0.5) are inside
    c.clear(0);
    c.disc(10, 10, 1, paint(0xFFFFFF));
    assert(countNot(c, BG) == 4);
    assert(px(c, 9, 9) == W && px(c, 10, 9) == W && px(c, 9, 10) == W && px(c, 10, 10) == W);
    assert(px(c, 10, 8) == BG);   // centre (10.5, 8.5): d^2 = 0.25 + 2.25 > 1
    // disc r=3: half-offsets 0.5/1.5/2.5 pair up as 8 per quadrant (2.5,2.5 gives 12.5 > 9)
    c.clear(0);
    c.disc(10, 10, 3, paint(0xFFFFFF));
    assert(countNot(c, BG) == 32);
    assert(px(c, 7, 8) == W);    // (-2.5, -1.5): 8.5 <= 9
    assert(px(c, 7, 7) == BG);   // (-2.5, -2.5): 12.5
    assert(px(c, 6, 10) == BG);  // (-3.5, 0.5): 12.5

    // horizontal segment (10,10)-(20,10) r=0.5: rows 9 and 10 (centres 9.5 and 10.5 are at distance 0.5),
    // columns 10..19 (centre 9.5 is 0.707 from the endpoint (10,10), 20.5 likewise from (20,10))
    c.clear(0);
    c.segment(10, 10, 20, 10, 0.5f, paint(0xFFFFFF));
    assert(countNot(c, BG) == 20);
    assert(px(c, 10, 9) == W && px(c, 19, 10) == W);
    assert(px(c, 9, 9) == BG && px(c, 20, 9) == BG && px(c, 10, 8) == BG && px(c, 10, 11) == BG);
    // r=1: endpoints now reach 9 and 20 (0.707 <= 1), rows still 9..10 (8.5 is 1.5 away)
    c.clear(0);
    c.segment(10, 10, 20, 10, 1, paint(0xFFFFFF));
    assert(countNot(c, BG) == 24);
    assert(px(c, 9, 9) == W && px(c, 20, 10) == W && px(c, 8, 9) == BG && px(c, 21, 10) == BG && px(c, 15, 8) == BG);
    // diagonal (10,10)-(20,20) r=0.5: only centres on the line, (px+0.5, px+0.5) for px 10..19;
    // the neighbour (11,10) is 1/sqrt(2) = 0.707 off the line
    c.clear(0);
    c.segment(10, 10, 20, 20, 0.5f, paint(0xFFFFFF));
    assert(countNot(c, BG) == 10);
    for (int i = 10; i < 20; i++) assert(px(c, i, i) == W);
    assert(px(c, 9, 9) == BG && px(c, 20, 20) == BG && px(c, 11, 10) == BG);
    // diagonal r=1: 0.707 neighbours and endpoint corners come in, 1.414 and 2.12 stay out
    c.clear(0);
    c.segment(10, 10, 20, 20, 1, paint(0xFFFFFF));
    assert(px(c, 11, 10) == W && px(c, 9, 9) == W && px(c, 20, 20) == W && px(c, 9, 10) == W);
    assert(px(c, 12, 10) == BG && px(c, 8, 8) == BG && px(c, 21, 21) == BG);

    // rect (10,10)-(20,20) r=1, sdBox at pixel centres: centre 9.5 gives dx = |9.5-15|-5 = 0.5 (in),
    // centre 8.5 gives 1.5 (out); corner (9,9) is sqrt(0.5) = 0.707 (in); (8,8) is 2.12 (out).
    c.clear(0);
    c.rect(10, 10, 20, 20, 1, paint(0xFFFFFF));
    assert(countNot(c, BG) == 12 * 12);
    assert(px(c, 9, 15) == W && px(c, 20, 15) == W && px(c, 15, 9) == W && px(c, 15, 20) == W && px(c, 15, 15) == W);
    assert(px(c, 8, 15) == BG && px(c, 21, 15) == BG && px(c, 15, 8) == BG && px(c, 15, 21) == BG);
    assert(px(c, 9, 9) == W && px(c, 20, 20) == W && px(c, 8, 8) == BG && px(c, 21, 9) == BG);
    // with r=2 the corner (8,8) is 1.5*sqrt(2) = 2.12 out, but (8,9) is sqrt(1.5^2+0.5^2) = 1.58 in
    c.clear(0);
    c.rect(10, 10, 20, 20, 2, paint(0xFFFFFF));
    assert(px(c, 8, 8) == BG && px(c, 8, 9) == W && px(c, 8, 15) == W && px(c, 7, 15) == BG);
    // swapped vertices paint the same box
    Canvas s(800, 480);
    s.rect(20, 20, 10, 10, 1, paint(0xFFFFFF));
    c.clear(0);
    c.rect(10, 10, 20, 20, 1, paint(0xFFFFFF));
    assert(memcmp(c.pixels(), s.pixels(), 800 * 480 * 4) == 0);

    // scissor: clip {12,0,5,480} keeps columns 12..16 only
    c.clear(0);
    c.rect(10, 10, 20, 20, 1, paint(0xFFFFFF, 255, BF_SRC_ALPHA, BF_ONE_MINUS_SRC_ALPHA, Clip{12, 0, 5, 480}));
    assert(countNot(c, BG) == 5 * 12);
    assert(px(c, 11, 15) == BG && px(c, 12, 15) == W && px(c, 16, 15) == W && px(c, 17, 15) == BG);
    // clip entirely off-canvas or empty draws nothing, and a shape off the canvas edge does not crash
    c.clear(0);
    c.rect(10, 10, 20, 20, 1, paint(0xFFFFFF, 255, BF_SRC_ALPHA, BF_ONE_MINUS_SRC_ALPHA, Clip{-50, -50, 10, 10}));
    c.disc(0, 0, 5, paint(0xFFFFFF));
    c.disc(800, 480, 5, paint(0xFFFFFF));
    assert(px(c, 0, 0) == W && px(c, 799, 479) == W);

    // blend Normal at a=128 on black: (255*128 + 127)/255 = 128
    c.clear(0);
    c.rect(0, 0, 1, 1, 0, paint(0xFFFFFF, 128));
    assert(px(c, 0, 0) == 0xFF808080u);
    // Normal a=128, white over 0x404040: (255*128 + 64*127 + 127)/255 = 40895/255 = 160
    c.clear(0x404040);
    c.rect(0, 0, 1, 1, 0, paint(0xFFFFFF, 128));
    assert(px(c, 0, 0) == 0xFFA0A0A0u);
    // Replace (SRC_ALPHA, ZERO) a=128 ignores the white destination: 128
    c.clear(0xFFFFFF);
    c.rect(0, 0, 1, 1, 0, paint(0xFFFFFF, 128, BF_SRC_ALPHA, BF_ZERO));
    assert(px(c, 0, 0) == 0xFF808080u);
    // Additive (SRC_ALPHA, ONE) a=255: 0x80 + 0xC0 clamps to 0xFF, other channels 0x40 + 0x20
    c.clear(0xC00040);
    c.rect(0, 0, 1, 1, 0, paint(0x800020, 255, BF_SRC_ALPHA, BF_ONE));
    assert(px(c, 0, 0) == 0xFFFF0060u);

    // gradient x 0..10: endpoints exact, midpoint (a*50 + b*50 + 50)/100 = 128 per channel, beyond x1 clamped
    c.clear(0);
    c.gradient(Clip{0, 0, 16, 2}, 0, 0, 0x102030, 10, 0, 0xF0E0D0, paint(0, 255, BF_SRC_ALPHA, BF_ZERO));
    assert(px(c, 0, 0) == 0xFF102030u && px(c, 0, 1) == 0xFF102030u);
    assert(px(c, 10, 0) == 0xFFF0E0D0u && px(c, 15, 1) == 0xFFF0E0D0u);
    assert(px(c, 5, 0) == 0xFF808080u);
    assert(px(c, 16, 0) == BG && px(c, 0, 2) == BG);   // area respected
    // gradient area is also scissored
    c.clear(0);
    c.gradient(Clip{0, 0, 16, 2}, 0, 0, 0xFFFFFF, 10, 0, 0xFFFFFF, paint(0, 255, BF_SRC_ALPHA, BF_ZERO, Clip{4, 0, 4, 1}));
    assert(countNot(c, BG) == 4 && px(c, 4, 0) == W && px(c, 7, 0) == W);

    // bitmap L4 4x2, stride 2: row0 A5 3C -> 170 85 51 204 ; row1 F0 0F -> 255 0 0 255.
    // White paint with Replace makes the pixel equal to L on every channel (L=0 writes black).
    const Transform ident = {256, 0, 0, 0, 256, 0};
    const uint8_t l4[] = {0xA5, 0x3C, 0xF0, 0x0F};
    const BitmapDesc l4d = {0, BM_L4, 2, 4, 2, 0, 0, 0};
    c.clear(0x123456);
    c.bitmap(l4d, l4, 10, 10, ident, paint(0xFFFFFF, 255, BF_SRC_ALPHA, BF_ZERO));
    assert(px(c, 10, 10) == 0xFFAAAAAAu && px(c, 11, 10) == 0xFF555555u && px(c, 12, 10) == 0xFF333333u && px(c, 13, 10) == 0xFFCCCCCCu);
    assert(px(c, 10, 11) == W && px(c, 11, 11) == 0xFF000000u && px(c, 12, 11) == 0xFF000000u && px(c, 13, 11) == W);
    assert(px(c, 14, 10) == 0xFF123456u && px(c, 10, 12) == 0xFF123456u && px(c, 9, 10) == 0xFF123456u);   // outside the drawn size: untouched
    // L4 alpha multiplies paint.a: L=170 at a=128 -> alpha (170*128+127)/255 = 85; Normal over black: (255*85+127)/255 = 85
    c.clear(0);
    c.bitmap(l4d, l4, 0, 0, ident, paint(0xFFFFFF, 128));
    assert(px(c, 0, 0) == 0xFF555555u);

    // bitmap RGB565 little-endian: F800 -> FF0000, 1234 -> r=2 g=17 b=20 -> 10 45 A5
    const uint8_t rgb565[] = {0x00, 0xF8, 0x34, 0x12};
    const BitmapDesc d565 = {0, BM_RGB565, 4, 2, 1, 0, 0, 0};
    c.clear(0);
    c.bitmap(d565, rgb565, 0, 0, ident, paint(0x00FF00, 255, BF_SRC_ALPHA, BF_ZERO));   // paint colour ignored for RGB formats
    assert(px(c, 0, 0) == 0xFFFF0000u && px(c, 1, 0) == 0xFF1045A5u);

    // 2x upscale (a=e=128): drawn 4x4 shows source 2x2, each source pixel covering 2x2
    uint8_t l8[16];
    for (int i = 0; i < 16; i++) l8[i] = (uint8_t)(i * 16 + 1);   // row*64 + col*16 + 1, all non-zero
    const BitmapDesc l8d = {0, BM_L8, 4, 4, 4, 0, 0, 0};
    c.clear(0);
    c.bitmap(l8d, l8, 0, 0, Transform{128, 0, 0, 0, 128, 0}, paint(0xFFFFFF, 255, BF_SRC_ALPHA, BF_ZERO));
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            uint32_t v = l8[(y / 2) * 4 + (x / 2)];
            assert(px(c, x, y) == (0xFF000000u | v << 16 | v << 8 | v));
        }
    assert(px(c, 4, 0) == BG && px(c, 0, 4) == BG);

    // slight downscale (a=e=276): sx = floor((px+0.5)*276/256): px 11 -> 12.4 -> 12, px 12 -> 13.47 -> 13,
    // px 15 -> 16.7 -> 16 is outside the 16-wide source, BORDER skips it
    uint8_t l8w[16];
    for (int i = 0; i < 16; i++) l8w[i] = (uint8_t)(i * 17);
    const BitmapDesc l8wd = {0, BM_L8, 16, 16, 1, 0, 0, 0};
    c.clear(0x123456);
    c.bitmap(l8wd, l8w, 0, 0, Transform{276, 0, 0, 0, 276, 0}, paint(0xFFFFFF, 255, BF_SRC_ALPHA, BF_ZERO));
    assert(px(c, 0, 0) == 0xFF000000u && px(c, 11, 0) == 0xFFCCCCCCu && px(c, 12, 0) == 0xFFDDDDDDu);
    assert(px(c, 15, 0) == 0xFF123456u);
    assert(px(c, 0, 1) == 0xFF123456u);   // height 1: row 1 not drawn

    // readPixels BGRA byte order, with stride
    c.clear(0);
    c.rect(3, 2, 4, 3, 0, paint(0x112233, 255, BF_SRC_ALPHA, BF_ZERO));
    assert(px(c, 3, 2) == 0xFF112233u);
    uint8_t out[3 * 16];
    memset(out, 0xEE, sizeof out);
    c.readPixels(2, 1, 3, 3, out, 16);
    const uint8_t* p = out + 1 * 16 + 1 * 4;
    assert(p[0] == 0x33 && p[1] == 0x22 && p[2] == 0x11 && p[3] == 0xFF);
    assert(out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 0xFF);
    assert(out[12] == 0xEE);   // bytes past w*4 in a row untouched

    puts("canvas_test: all checks passed");
    return 0;
}
