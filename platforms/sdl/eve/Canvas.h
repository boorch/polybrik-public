// Software 2D surface for the FT812 emulator.
// Pixels are 0xAARRGGBB with AA always 0xFF (SDL_PIXELFORMAT_ARGB8888 on a
// little-endian host). Shapes use EVE distance semantics: the pixel whose
// centre (px+0.5, py+0.5) lies within `radius` of the ideal shape is painted.
// v1 is binary coverage.
#pragma once

#include <cstdint>
#include <vector>

struct Clip { int x, y, w, h; };
// srcF/dstF are EVE BLEND_FUNC factors.
enum : uint8_t { BF_ZERO = 0, BF_ONE = 1, BF_SRC_ALPHA = 2, BF_DST_ALPHA = 3,
                 BF_ONE_MINUS_SRC_ALPHA = 4, BF_ONE_MINUS_DST_ALPHA = 5 };
struct Paint { uint32_t rgb; uint8_t a; uint8_t srcF, dstF; Clip clip; };

// EVE BITMAP_LAYOUT formats used by the firmware.
enum : uint8_t { BM_L1 = 1, BM_L4 = 2, BM_L8 = 3, BM_RGB332 = 4, BM_RGB565 = 7, BM_L2 = 17 };
struct BitmapDesc { uint32_t source; uint8_t format; uint16_t stride, width, height; uint8_t filter, wrapx, wrapy; };
// DL fixed point, screen -> bitmap: a,b,d,e are 8.8, c,f are 15.8. Identity {256,0,0,0,256,0}.
struct Transform { int32_t a, b, c, d, e, f; };

class Canvas {
public:
    Canvas(int w, int h);
    void clear(uint32_t rgb);
    void disc(float cx, float cy, float radius, const Paint&);
    void segment(float x0, float y0, float x1, float y1, float radius, const Paint&);
    // RECTS: box expanded by radius with rounded corners (Minkowski sum with a disc).
    void rect(float x0, float y0, float x1, float y1, float radius, const Paint&);
    // CMD_GRADIENT: colour interpolated along (x0,y0)->(x1,y1), clamped at both ends, alpha = paint.a.
    void gradient(const Clip& area, int x0, int y0, uint32_t c0, int x1, int y1, uint32_t c1, const Paint&);
    // BITMAPS vertex: mem points at the bitmap's first byte (source and cell offset already resolved).
    void bitmap(const BitmapDesc& bm, const uint8_t* mem, int x, int y, const Transform& t, const Paint&);
    void readPixels(int x, int y, int w, int h, uint8_t* dstBGRA, int dstStrideBytes);
    const uint32_t* pixels() const { return buf.data(); }
    int width() const { return W; }
    int height() const { return H; }

private:
    // The only pixel writer. Honours nothing but its arguments: the shape functions clip.
    void blend(int px, int py, uint32_t rgb, uint8_t a, uint8_t srcF, uint8_t dstF);
    // Intersection of paint.clip with the canvas and [bx0,bx1)x[by0,by1); false when empty.
    bool clipBox(const Clip& c, int& bx0, int& by0, int& bx1, int& by1) const;
    std::vector<uint32_t> buf;
    int W, H;
};
