#include "io/Png.h"
#include "eve/Canvas.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

#include <cstdint>
#include <vector>

bool savePng(const Canvas& canvas, const char* path)
{
    const int w = canvas.width(), h = canvas.height();
    const uint32_t* px = canvas.pixels();
    std::vector<uint8_t> rgb(static_cast<size_t>(w) * h * 3);
    for (size_t i = 0, n = static_cast<size_t>(w) * h; i < n; i++)
    {
        rgb[i * 3 + 0] = (px[i] >> 16) & 0xFF;
        rgb[i * 3 + 1] = (px[i] >> 8) & 0xFF;
        rgb[i * 3 + 2] = px[i] & 0xFF;
    }
    return stbi_write_png(path, w, h, 3, rgb.data(), w * 3) != 0;
}
