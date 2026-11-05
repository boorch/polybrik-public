#include "io/sdl/SdlDisplayOut.h"
#include "eve/Canvas.h"

#include <cstdio>
#include <cstdlib>

static const int WIN_W = 1024, WIN_H = 768;
static const int STRIP_W = 1024, STRIP_H = 154;   // below the 1024x614 canvas

SdlDisplayOut::SdlDisplayOut(int scale, bool fullscreen)
{
    // 1.28x linear at the top (154 px strip), or 1:1 above the strip
    setUnscaled(scale == 1);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    window = SDL_CreateWindow("PolyBrik", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H,
                              fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (window)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer)
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 800, 480);
    if (texture)   // also when 1:1: L4 + Menu can switch back to the stretched layout at runtime
        strip = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, STRIP_W, STRIP_H);
    if (!texture || !strip)
    {
        fprintf(stderr, "SdlDisplayOut: %s\n", SDL_GetError());
        exit(1);
    }
    // fullscreen-desktop may not be 1024x768: keep dst in window units and let SDL scale
    SDL_RenderSetLogicalSize(renderer, WIN_W, WIN_H);
}

SdlDisplayOut::~SdlDisplayOut()
{
    if (strip) SDL_DestroyTexture(strip);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

void SdlDisplayOut::present(const Canvas& canvas)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (!blank)
    {
        SDL_UpdateTexture(texture, nullptr, canvas.pixels(), canvas.width() * 4);
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        if (hasStrip)
        {
            const SDL_Rect sd{0, WIN_H - STRIP_H, STRIP_W, STRIP_H};
            SDL_RenderCopy(renderer, strip, nullptr, &sd);
        }
    }
    SDL_RenderPresent(renderer);
}

void SdlDisplayOut::setUnscaled(bool unscaled)
{
    // 1:1 sits centred directly on top of the legend strip, not at the top of the screen
    dst = unscaled ? SDL_Rect{(WIN_W - 800) / 2, WIN_H - STRIP_H - 480, 800, 480} : SDL_Rect{0, 0, 1024, 614};
}

void SdlDisplayOut::setBrightness(int v)
{
    if (v < 0) v = 0;
    if (v > 128) v = 128;   // cDisplay's field defaults to 255 before config sets 20/50/128
    const Uint8 m = static_cast<Uint8>(v * 255 / 128);
    SDL_SetTextureColorMod(texture, m, m, m);
    if (strip) SDL_SetTextureColorMod(strip, m, m, m);   // the backlight dims the whole panel
}

void SdlDisplayOut::presentStrip(const uint32_t* pixels, int w, int h)
{
    if (!strip || w != STRIP_W || h != STRIP_H) return;
    SDL_UpdateTexture(strip, nullptr, pixels, w * 4);
    hasStrip = true;
}

void SdlDisplayOut::setBlank(bool b)
{
    blank = b;
    if (blank)
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
}
