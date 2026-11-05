#include "io/raw/FbdevDisplayOut.h"
#include "eve/Canvas.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

uint32_t FbdevDisplayOut::pack(uint32_t argb, const uint8_t l[256], int ro, int go, int bo, int to)
{
    uint32_t v = (uint32_t)l[(argb >> 16) & 0xFF] << ro | (uint32_t)l[(argb >> 8) & 0xFF] << go |
                 (uint32_t)l[argb & 0xFF] << bo;
    return to >= 0 ? v | 0xFFu << to : v;   // opaque: the display layer may blend by pixel alpha
}

FbdevDisplayOut::FbdevDisplayOut(const char* path, bool unscaled_) : unscaled(unscaled_)
{
    for (int i = 0; i < 256; i++) lut[i] = (uint8_t)i;
    fb_var_screeninfo v{};
    fb_fix_screeninfo f{};
    fd = open(path, O_RDWR);
    if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f))
    {
        perror(path);
        exit(1);
    }
    if (v.bits_per_pixel != 32 || v.red.length != 8 || v.green.length != 8 || v.blue.length != 8)
    {
        fprintf(stderr, "FbdevDisplayOut: %s is %u bpp (r%u g%u b%u), only 32 bpp 8:8:8 is supported\n", path,
                v.bits_per_pixel, v.red.length, v.green.length, v.blue.length);
        exit(1);
    }
    xres = (int)v.xres;
    yres = (int)v.yres;
    stridePx = (int)(f.line_length / 4);
    roff = (int)v.red.offset;
    goff = (int)v.green.offset;
    boff = (int)v.blue.offset;
    toff = v.transp.length == 8 ? (int)v.transp.offset : -1;
    canPan = v.yres_virtual >= 2 * v.yres;
    memLen = (size_t)f.line_length * (canPan ? 2 * v.yres : v.yres);   // only the pages we use
    mem = (uint8_t*)mmap(nullptr, memLen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED)
    {
        perror("FbdevDisplayOut: mmap");
        exit(1);
    }
    setUnscaled(unscaled);
    memset(mem, 0, memLen);
    back = canPan ? 1 : 0;
    fprintf(stderr, "fbdev: %s %dx%d stride %d px, rgba offsets %d/%d/%d/%d, %s\n", path, xres, yres, stridePx,
            roff, goff, boff, toff, canPan ? "page flip" : "single buffer");
    if (canPan)
    {
        v.xoffset = v.yoffset = 0;
        ioctl(fd, FBIOPAN_DISPLAY, &v);
    }
}

FbdevDisplayOut::~FbdevDisplayOut()
{
    if (mem && mem != MAP_FAILED)
    {
        memset(mem, 0, memLen);
        if (canPan)
        {
            fb_var_screeninfo v{};
            if (!ioctl(fd, FBIOGET_VSCREENINFO, &v))
            {
                v.xoffset = v.yoffset = 0;
                ioctl(fd, FBIOPAN_DISPLAY, &v);
            }
        }
        munmap(mem, memLen);
    }
    if (fd >= 0) close(fd);
}

uint32_t* FbdevDisplayOut::page(int n) const
{
    return (uint32_t*)(mem + (size_t)n * yres * stridePx * 4);
}

void FbdevDisplayOut::flip()
{
    if (!canPan) return;
    fb_var_screeninfo v{};
    if (ioctl(fd, FBIOGET_VSCREENINFO, &v)) return;
    v.xoffset = 0;
    v.yoffset = (uint32_t)(back * yres);
    if (ioctl(fd, FBIOPAN_DISPLAY, &v)) perror("FBIOPAN_DISPLAY");
    back ^= 1;
}

void FbdevDisplayOut::present(const Canvas& canvas)
{
    uint32_t* dst = page(back);
    if (blank)
    {
        memset(dst, 0, (size_t)yres * stridePx * 4);
        flip();
        return;
    }
    const int cw = canvas.width(), ch = canvas.height();
    if ((int)xlut.size() != xres || (xres && xlut[xres - 1] >= cw))
    {
        xlut.resize(xres);
        const int xoff = (xres - cw) / 2;   // unscaled: centred; -1 marks a black column
        for (int x = 0; x < xres; x++)
            xlut[x] = !unscaled ? x * cw / xres : (x >= xoff && x < xoff + cw) ? x - xoff : -1;
    }
    if (frame.size() != (size_t)xres * yres)
    {
        frame.assign((size_t)xres * yres, 0);
        stripDirty = true;
    }
    // Canvas pixels are 0xFFRRGGBB: at full brightness in the canvas's own layout pack() is the identity.
    const bool direct = identity && roff == 16 && goff == 8 && boff == 0 && toff == 24;
    const uint32_t* src = canvas.pixels();
    // Unscaled: the canvas sits directly on top of the strip; stretched: at the top.
    const bool stripFits = !strip.empty() && stripW <= xres && stripH <= yres;
    const int wantY = unscaled ? std::max(0, (stripFits ? yres - stripH : yres) - canvasH) : 0;
    if (wantY != canvasY)
    {
        canvasY = wantY;
        stripDirty = true;
    }
    int lastSy = -1;
    for (int y = 0; y < canvasH; y++)
    {
        uint32_t* row = &frame[(size_t)(canvasY + y) * xres];
        const int sy = y * ch / canvasH;
        if (sy == lastSy)
        {
            memcpy(row, row - xres, (size_t)xres * 4);   // nearest neighbour: repeat the row (cached RAM)
            continue;
        }
        lastSy = sy;
        const uint32_t* s = src + (size_t)sy * cw;
        if (unscaled)
            for (int x = 0; x < xres; x++) row[x] = xlut[x] < 0 ? 0 : direct ? s[xlut[x]] : pack(s[xlut[x]], lut, roff, goff, boff, toff);
        else if (direct)
            for (int x = 0; x < xres; x++) row[x] = s[xlut[x]];
        else
            for (int x = 0; x < xres; x++) row[x] = pack(s[xlut[x]], lut, roff, goff, boff, toff);
    }
    // Outside the canvas: black, with the strip at the bottom when it fits. Only rebuilt when it changed.
    if (stripDirty)
    {
        stripDirty = false;
        const int stripY = yres - stripH;
        const bool drawStrip = stripFits && stripY >= canvasY + canvasH;
        for (int y = 0; y < yres; y++)
        {
            if (y >= canvasY && y < canvasY + canvasH) continue;   // canvas rows are written every frame
            uint32_t* row = &frame[(size_t)y * xres];
            if (drawStrip && y >= stripY)
            {
                const uint32_t* s = strip.data() + (size_t)(y - stripY) * stripW;
                for (int x = 0; x < stripW; x++) row[x] = pack(s[x], lut, roff, goff, boff, toff);
                memset(row + stripW, 0, (size_t)(xres - stripW) * 4);
            }
            else
                memset(row, 0, (size_t)xres * 4);
        }
    }
    // One sequential write into the page, never a read of framebuffer memory.
    if (stridePx == xres)
        memcpy(dst, frame.data(), frame.size() * 4);
    else
        for (int y = 0; y < yres; y++) memcpy(dst + (size_t)y * stridePx, &frame[(size_t)y * xres], (size_t)xres * 4);
    flip();
}

void FbdevDisplayOut::setUnscaled(bool u)
{
    unscaled = u;
    canvasH = unscaled ? 480 : xres * 3 / 5;   // 800x480 keeps its aspect: 1024 -> 614
    if (canvasH > yres) canvasH = yres;
    xlut.clear();        // rebuilt for the new mode on the next present
    stripDirty = true;   // the rows around the canvas change
}

void FbdevDisplayOut::presentStrip(const uint32_t* pixels, int w, int h)
{
    if (w != stripW || h != stripH || !std::equal(pixels, pixels + (size_t)w * h, strip.begin()))
    {
        strip.assign(pixels, pixels + (size_t)w * h);
        stripW = w;
        stripH = h;
        stripDirty = true;
    }
}

void FbdevDisplayOut::setBrightness(int v)
{
    if (v < 0) v = 0;
    if (v > 128) v = 128;   // same scale as SdlDisplayOut: the firmware's backlight value
    for (int i = 0; i < 256; i++) lut[i] = (uint8_t)(i * v / 128);
    identity = v == 128;
    stripDirty = true;   // the strip goes through the LUT too
}

void FbdevDisplayOut::setBlank(bool b)
{
    blank = b;
    if (blank)
    {
        memset(page(back), 0, (size_t)yres * stridePx * 4);
        flip();
    }
}
