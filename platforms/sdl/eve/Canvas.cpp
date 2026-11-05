#include "eve/Canvas.h"

#include <algorithm>
#include <cmath>

Canvas::Canvas(int w, int h) : buf((size_t)w * h, 0xFF000000u), W(w), H(h) {}

void Canvas::clear(uint32_t rgb)
{
    std::fill(buf.begin(), buf.end(), 0xFF000000u | (rgb & 0xFFFFFFu));
}

bool Canvas::clipBox(const Clip& c, int& bx0, int& by0, int& bx1, int& by1) const
{
    bx0 = std::max({bx0, c.x, 0});
    by0 = std::max({by0, c.y, 0});
    bx1 = std::min({bx1, c.x + c.w, W});
    by1 = std::min({by1, c.y + c.h, H});
    return bx0 < bx1 && by0 < by1;
}

// Blend factors in 8-bit fixed point: factor 255 == 1.0, da == 1 (opaque screen).
static int blendFactor(uint8_t f, uint8_t a)
{
    switch (f) {
        case BF_ZERO: return 0;
        case BF_ONE: return 255;
        case BF_SRC_ALPHA: return a;
        case BF_DST_ALPHA: return 255;
        case BF_ONE_MINUS_SRC_ALPHA: return 255 - a;
        default: return 0;   // ONE_MINUS_DST_ALPHA
    }
}

// fs 255 / fd 0 gives (s*255+127)/255 == s and fs 0 / fd 255 gives d, so those two
// cases (opaque and fully transparent pixels, nearly all of them) skip the divisions.
static inline void blendPixel(uint32_t& dst, uint32_t rgb, int fs, int fd)
{
    if (fs == 255 && fd == 0) { dst = 0xFF000000u | (rgb & 0xFFFFFFu); return; }
    if (fs == 0 && fd == 255) return;
    uint32_t out = 0xFF000000u;
    for (int shift = 0; shift <= 16; shift += 8) {
        int sc = (rgb >> shift) & 0xFF, d = (dst >> shift) & 0xFF;
        int o = std::min(255, (sc * fs + d * fd + 127) / 255);
        out |= (uint32_t)o << shift;
    }
    dst = out;
}

void Canvas::blend(int px, int py, uint32_t rgb, uint8_t a, uint8_t srcF, uint8_t dstF)
{
    blendPixel(buf[(size_t)py * W + px], rgb, blendFactor(srcF, a), blendFactor(dstF, a));
}

void Canvas::disc(float cx, float cy, float r, const Paint& p)
{
    int bx0 = (int)std::floor(cx - r), by0 = (int)std::floor(cy - r);
    int bx1 = (int)std::ceil(cx + r), by1 = (int)std::ceil(cy + r);
    if (!clipBox(p.clip, bx0, by0, bx1, by1)) return;
    const float r2 = r * r;
    for (int py = by0; py < by1; py++)
        for (int px = bx0; px < bx1; px++) {
            float dx = px + 0.5f - cx, dy = py + 0.5f - cy;
            if (dx * dx + dy * dy <= r2) blend(px, py, p.rgb, p.a, p.srcF, p.dstF);
        }
}

void Canvas::segment(float x0, float y0, float x1, float y1, float r, const Paint& p)
{
    int bx0 = (int)std::floor(std::min(x0, x1) - r), by0 = (int)std::floor(std::min(y0, y1) - r);
    int bx1 = (int)std::ceil(std::max(x0, x1) + r), by1 = (int)std::ceil(std::max(y0, y1) + r);
    if (!clipBox(p.clip, bx0, by0, bx1, by1)) return;
    const float ex = x1 - x0, ey = y1 - y0, len2 = ex * ex + ey * ey, r2 = r * r;
    for (int py = by0; py < by1; py++)
        for (int px = bx0; px < bx1; px++) {
            float qx = px + 0.5f - x0, qy = py + 0.5f - y0;
            float t = len2 > 0.f ? std::clamp((qx * ex + qy * ey) / len2, 0.f, 1.f) : 0.f;
            float dx = qx - t * ex, dy = qy - t * ey;
            if (dx * dx + dy * dy <= r2) blend(px, py, p.rgb, p.a, p.srcF, p.dstF);
        }
}

void Canvas::rect(float x0, float y0, float x1, float y1, float r, const Paint& p)
{
    int bx0 = (int)std::floor(std::min(x0, x1) - r), by0 = (int)std::floor(std::min(y0, y1) - r);
    int bx1 = (int)std::ceil(std::max(x0, x1) + r), by1 = (int)std::ceil(std::max(y0, y1) + r);
    if (!clipBox(p.clip, bx0, by0, bx1, by1)) return;
    const int fs = blendFactor(p.srcF, p.a), fd = blendFactor(p.dstF, p.a);
    const uint32_t opaque = 0xFF000000u | (p.rgb & 0xFFFFFFu);
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hx = fabsf(x1 - x0) * 0.5f, hy = fabsf(y1 - y0) * 0.5f;
    for (int py = by0; py < by1; py++) {
        const float dy = fabsf(py + 0.5f - cy) - hy;
        // The box's signed distance without the square root where at most one of the
        // outside offsets is non-zero: sqrtf(v*v) == v exactly in IEEE arithmetic.
        auto inside = [&](int px) {
            const float dx = fabsf(px + 0.5f - cx) - hx;
            float d;
            if (dx > 0.f && dy > 0.f) d = sqrtf(dx * dx + dy * dy);
            else if (dx > 0.f) d = dx;
            else if (dy > 0.f) d = dy;
            else d = fmaxf(dx, dy);
            return d <= r;
        };
        // For a fixed row the distance only grows with |px + 0.5 - cx|, so the painted pixels are one run:
        // walk in from both ends with the exact test, then fill the run.
        int l = bx0, h = bx1 - 1;
        while (l <= h && !inside(l)) l++;
        while (h >= l && !inside(h)) h--;
        if (l > h) continue;
        uint32_t* row = &buf[(size_t)py * W];
        if (fs == 255 && fd == 0) std::fill(row + l, row + h + 1, opaque);
        else for (int px = l; px <= h; px++) blendPixel(row[px], p.rgb, fs, fd);
    }
}

void Canvas::gradient(const Clip& area, int x0, int y0, uint32_t c0, int x1, int y1, uint32_t c1, const Paint& p)
{
    int bx0 = area.x, by0 = area.y, bx1 = area.x + area.w, by1 = area.y + area.h;
    if (!clipBox(p.clip, bx0, by0, bx1, by1)) return;
    const int ex = x1 - x0, ey = y1 - y0, den = ex * ex + ey * ey;
    for (int py = by0; py < by1; py++)
        for (int px = bx0; px < bx1; px++) {
            // Integer pixel coordinates so the pixel at (x0,y0) is exactly c0 and (x1,y1) exactly c1.
            int num = den ? std::clamp((px - x0) * ex + (py - y0) * ey, 0, den) : 0;
            uint32_t rgb = 0;
            for (int shift = 0; shift <= 16; shift += 8) {
                int a = (c0 >> shift) & 0xFF, b = (c1 >> shift) & 0xFF;
                int c = den ? (a * (den - num) + b * num + den / 2) / den : a;
                rgb |= (uint32_t)c << shift;
            }
            blend(px, py, rgb, p.a, p.srcF, p.dstF);
        }
}

// One bitmap pixel: rgb and lum (0..255) for formats the firmware uses; false for an unknown format.
static inline bool bitmapTexel(uint8_t format, const uint8_t* row, int sx, uint32_t& rgb, int& lum)
{
    switch (format) {
        case BM_L1: lum = (row[sx >> 3] >> (7 - (sx & 7))) & 1 ? 255 : 0; return true;
        case BM_L2: lum = ((row[sx >> 2] >> (6 - 2 * (sx & 3))) & 3) * 85; return true;
        case BM_L4: lum = ((sx & 1) ? (row[sx >> 1] & 0xF) : (row[sx >> 1] >> 4)) * 17; return true;
        case BM_L8: lum = row[sx]; return true;
        case BM_RGB332: {
            uint8_t v = row[sx];
            int r = (v >> 5) & 7, g = (v >> 2) & 7, b = v & 3;
            rgb = ((r * 255 / 7) << 16) | ((g * 255 / 7) << 8) | (b * 255 / 3);
            return true;
        }
        case BM_RGB565: {
            uint16_t v = row[2 * sx] | (row[2 * sx + 1] << 8);
            int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
            rgb = (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
            return true;
        }
        default: return false;   // unknown format: draw nothing
    }
}

template<uint8_t Format>
static inline void bitmapRun(uint32_t* dst, const uint8_t* row, int sx0, int n, const Paint& p, const int f[4])
{
    for (int i = 0; i < n; i++) {
        uint32_t rgb = p.rgb;
        int lum = 255;
        bitmapTexel(Format, row, sx0 + i, rgb, lum);
        // lum 255 gives (255 * a + 127) / 255 == a and lum 0 gives 0: the factors for both are precomputed.
        if (lum == 255) blendPixel(dst[i], rgb, f[0], f[1]);
        else if (lum == 0) blendPixel(dst[i], rgb, f[2], f[3]);
        else {
            const uint8_t a = (uint8_t)((lum * p.a + 127) / 255);
            blendPixel(dst[i], rgb, blendFactor(p.srcF, a), blendFactor(p.dstF, a));
        }
    }
}

void Canvas::bitmap(const BitmapDesc& bm, const uint8_t* mem, int x, int y, const Transform& t, const Paint& p)
{
    int bx0 = x, by0 = y, bx1 = x + bm.width, by1 = y + bm.height;
    if (!clipBox(p.clip, bx0, by0, bx1, by1)) return;
    const int f[4] = { blendFactor(p.srcF, p.a), blendFactor(p.dstF, p.a), blendFactor(p.srcF, 0), blendFactor(p.dstF, 0) };
    const bool identity = t.a == 256 && t.b == 0 && t.c == 0 && t.d == 0 && t.e == 256 && t.f == 0;
    if (identity) {
        // floor((256 * (n + 0.5)) / 256) == n: texel (px - x, py - y), always inside the bitmap after clipBox.
        for (int py = by0; py < by1; py++) {
            const uint8_t* row = mem + (size_t)(py - y) * bm.stride;
            uint32_t* dst = &buf[(size_t)py * W + bx0];
            const int sx0 = bx0 - x, n = bx1 - bx0;
            switch (bm.format) {
                case BM_L1: bitmapRun<BM_L1>(dst, row, sx0, n, p, f); break;
                case BM_L2: bitmapRun<BM_L2>(dst, row, sx0, n, p, f); break;
                case BM_L4: bitmapRun<BM_L4>(dst, row, sx0, n, p, f); break;
                case BM_L8: bitmapRun<BM_L8>(dst, row, sx0, n, p, f); break;
                case BM_RGB332: bitmapRun<BM_RGB332>(dst, row, sx0, n, p, f); break;
                case BM_RGB565: bitmapRun<BM_RGB565>(dst, row, sx0, n, p, f); break;
                default: return;   // unknown format: draw nothing
            }
        }
        return;
    }
    for (int py = by0; py < by1; py++)
        for (int px = bx0; px < bx1; px++) {
            float dx = px - x + 0.5f, dy = py - y + 0.5f;
            int sx = (int)std::floor((t.a * dx + t.b * dy + t.c) / 256.f);
            int sy = (int)std::floor((t.d * dx + t.e * dy + t.f) / 256.f);
            if (sx < 0 || sy < 0 || sx >= bm.width || sy >= bm.height) continue;   // BORDER wrap
            uint32_t rgb = p.rgb;
            int lum = 255;
            if (!bitmapTexel(bm.format, mem + (size_t)sy * bm.stride, sx, rgb, lum)) continue;
            const uint8_t a = lum == 255 ? p.a : lum == 0 ? 0 : (uint8_t)((lum * p.a + 127) / 255);
            blendPixel(buf[(size_t)py * W + px], rgb, blendFactor(p.srcF, a), blendFactor(p.dstF, a));
        }
}

void Canvas::readPixels(int x, int y, int w, int h, uint8_t* dstBGRA, int dstStrideBytes)
{
    for (int row = 0; row < h; row++) {
        uint8_t* d = dstBGRA + (size_t)row * dstStrideBytes;
        for (int col = 0; col < w; col++, d += 4) {
            int px = x + col, py = y + row;
            uint32_t v = (px >= 0 && py >= 0 && px < W && py < H) ? buf[(size_t)py * W + px] : 0xFF000000u;
            d[0] = v & 0xFF; d[1] = (v >> 8) & 0xFF; d[2] = (v >> 16) & 0xFF; d[3] = 0xFF;
        }
    }
}
