// Assert-based self-check of savePng: draw a known pattern, write it, read it back
// with stb_image and compare every pixel.
#undef NDEBUG
#include "eve/Canvas.h"
#include "io/Png.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main()
{
    Canvas canvas(800, 480);
    const Clip full{0, 0, 800, 480};
    Paint p{0, 255, BF_SRC_ALPHA, BF_ONE_MINUS_SRC_ALPHA, full};   // opaque

    canvas.clear(0x102030);
    p.rgb = 0xFF8000;
    canvas.rect(10, 10, 200, 100, 0, p);
    p.rgb = 0x00C0FF;
    canvas.rect(300, 200, 500, 400, 8, p);
    canvas.gradient(Clip{0, 420, 800, 60}, 0, 420, 0xFF0000, 799, 420, 0x0000FF, p);

    char path[] = "/tmp/png_test_XXXXXX.png";
    int fd = mkstemps(path, 4);
    assert(fd >= 0);
    close(fd);

    assert(savePng(canvas, path));

    int w = 0, h = 0, comp = 0;
    unsigned char* img = stbi_load(path, &w, &h, &comp, 3);
    assert(img);
    assert(w == 800 && h == 480);

    const uint32_t* px = canvas.pixels();
    size_t mismatches = 0;
    for (size_t i = 0; i < 800u * 480u; i++)
    {
        const uint32_t got = (uint32_t(img[i * 3]) << 16) | (uint32_t(img[i * 3 + 1]) << 8) | img[i * 3 + 2];
        if (got != (px[i] & 0xFFFFFF))
            mismatches++;
    }
    stbi_image_free(img);
    unlink(path);

    printf("png_test: %dx%d, %zu mismatching pixels\n", w, h, mismatches);
    assert(mismatches == 0);
    printf("png_test: OK\n");
    return 0;
}
