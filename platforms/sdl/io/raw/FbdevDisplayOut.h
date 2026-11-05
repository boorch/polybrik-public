// Linux framebuffer presenter for the T32 build: the same job as
// SdlDisplayOut at scale 0 (canvas scaled to the screen width at the top, legend strip at the
// bottom), written by the CPU into /dev/fb0. Page flipping with FBIOPAN_DISPLAY when the virtual
// height holds two screens. The framebuffer is blanked and panned back to 0 on destruction.
#pragma once
#include "io/IDisplayOut.h"

#include <cstdint>
#include <vector>

class FbdevDisplayOut : public IDisplayOut
{
public:
    // unscaled: the canvas 1:1, centred at the top, instead of stretched to the screen width. Exits on failure.
    explicit FbdevDisplayOut(const char* path = "/dev/fb0", bool unscaled = false);
    ~FbdevDisplayOut() override;

    void present(const Canvas& canvas) override;
    void setBrightness(int v) override;
    void setBlank(bool blank) override;
    void presentStrip(const uint32_t* pixels, int w, int h) override;
    void setUnscaled(bool unscaled) override;

    // One canvas pixel (0xAARRGGBB) through the brightness LUT into the framebuffer layout.
    static uint32_t pack(uint32_t argb, const uint8_t lut[256], int roff, int goff, int boff, int toff);

private:
    void flip();
    uint32_t* page(int n) const;

    int fd = -1;
    uint8_t* mem = nullptr;
    size_t memLen = 0;
    int xres = 0, yres = 0, stridePx = 0;
    int roff = 16, goff = 8, boff = 0, toff = 24;   // toff -1: no alpha channel
    bool canPan = false;
    int back = 0;           // page drawn next
    int canvasH = 0;        // scaled canvas height (614 on 1024x768; 480 unscaled)
    int canvasY = 0;        // top row of the canvas: 0 stretched; unscaled, directly above the strip
    bool unscaled = false;
    std::vector<int> xlut;  // destination x -> canvas x
    std::vector<uint32_t> strip;
    std::vector<uint32_t> frame;   // the next screen, built in cached RAM and copied to the page in one pass
    bool stripDirty = true;
    int stripW = 0, stripH = 0;
    uint8_t lut[256];       // brightness, per channel
    bool identity = true;
    bool blank = false;
};
