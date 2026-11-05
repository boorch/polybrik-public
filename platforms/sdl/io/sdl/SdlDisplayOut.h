// SDL2 window presenter: 1024x768 window, canvas at the top, strip below.
#pragma once
#include "io/IDisplayOut.h"

#include <SDL.h>

class SdlDisplayOut : public IDisplayOut
{
public:
    // scale 0: canvas scaled to 1024x614 at the top. scale 1: 800x480 at the top, horizontally centred.
    SdlDisplayOut(int scale, bool fullscreen);
    ~SdlDisplayOut() override;

    void present(const Canvas& canvas) override;
    void setBrightness(int v) override;
    void setBlank(bool blank) override;
    void setUnscaled(bool unscaled) override;
    // scale 0 only: the 1024x154 strip under the canvas. Ignored at scale 1 and for other sizes.
    void presentStrip(const uint32_t* pixels, int w, int h) override;

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_Texture* strip = nullptr;   // scale 0 only
    bool hasStrip = false;
    SDL_Rect dst;
    bool blank = false;
};
