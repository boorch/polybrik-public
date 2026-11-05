// Presenter interface: takes the finished 800x480 canvas and puts it on a screen.
// Events (window close, input) are not here; they belong to the input lane.
#pragma once

#include <cstdint>

class Canvas;

class IDisplayOut
{
public:
    virtual ~IDisplayOut() = default;
    virtual void present(const Canvas& canvas) = 0;
    virtual void setBrightness(int v) = 0;   // 0..128 as cDisplay::setBacklightBrightness passes it
    virtual void setBlank(bool blank) = 0;
    // Legend strip (0xFFRRGGBB, w x h), drawn with the next present(); presenters without a strip ignore it.
    virtual void presentStrip(const uint32_t* pixels, int w, int h) {}
    // true: the canvas 1:1, centred; false: stretched to the screen width (L4 + Menu toggles it at runtime).
    virtual void setUnscaled(bool unscaled) {}
};
