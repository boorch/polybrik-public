#include "eve/EveEmu.h"
#include "SimClock.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

#include "FT81x.h"

namespace {

constexpr uint32_t PRIV_GRADIENT_OP = 46;   // unused DL opcode, bits 31:30 = 00
constexpr uint32_t kCmdMask = 0xFFFFFF00u;
// Log-once slots outside the command id range (ids use their low byte, 0x00..0x43).
enum { LOG_MEM = 0x80, LOG_FONT, LOG_JPEG, LOG_RING, LOG_SNAPSHOT, LOG_IMGSIZE };

// Bytes each coprocessor command occupies in the FIFO, as the firmware writes them
// (core/libs/FT812/FT812.cpp IncCMDOffset values). 0 = 16 bytes plus a padded string.
struct CmdInfo { uint32_t id; uint8_t bytes; const char* name; };
const CmdInfo kCmds[] = {
    { CMD_DLSTART, 4, "DLSTART" },          { CMD_SWAP, 4, "SWAP" },
    { CMD_TEXT, 0, "TEXT" },                { CMD_BUTTON, 0, "BUTTON" },
    { CMD_KEYS, 0, "KEYS" },                { CMD_TOGGLE, 0, "TOGGLE" },
    { CMD_NUMBER, 16, "NUMBER" },           { CMD_LOADIDENTITY, 4, "LOADIDENTITY" },
    { CMD_GAUGE, 20, "GAUGE" },             { CMD_REGREAD, 12, "REGREAD" },
    { CMD_GETPROPS, 16, "GETPROPS" },       { CMD_MEMCPY, 16, "MEMCPY" },
    { CMD_SPINNER, 12, "SPINNER" },         { CMD_BGCOLOR, 8, "BGCOLOR" },
    { CMD_INFLATE, 8, "INFLATE" },          { CMD_TRANSLATE, 12, "TRANSLATE" },
    { CMD_STOP, 4, "STOP" },                { CMD_SLIDER, 20, "SLIDER" },
    { CMD_INTERRUPT, 8, "INTERRUPT" },      { CMD_FGCOLOR, 8, "FGCOLOR" },
    { CMD_ROTATE, 8, "ROTATE" },            { CMD_MEMWRITE, 12, "MEMWRITE" },
    { CMD_SCROLLBAR, 20, "SCROLLBAR" },     { CMD_GETMATRIX, 28, "GETMATRIX" },
    { CMD_SKETCH, 20, "SKETCH" },           { CMD_MEMSET, 16, "MEMSET" },
    { CMD_GRADCOLOR, 8, "GRADCOLOR" },      { CMD_BITMAP_TRANSFORM, 56, "BITMAP_TRANSFORM" },
    { CMD_CALIBRATE, 8, "CALIBRATE" },      { CMD_SETFONT, 12, "SETFONT" },
    { CMD_LOGO, 4, "LOGO" },                { CMD_APPEND, 12, "APPEND" },
    { CMD_MEMZERO, 12, "MEMZERO" },         { CMD_SCALE, 12, "SCALE" },
    { CMD_CLOCK, 20, "CLOCK" },             { CMD_GRADIENT, 20, "GRADIENT" },
    { CMD_SETMATRIX, 4, "SETMATRIX" },      { CMD_TRACK, 16, "TRACK" },
    { CMD_GETPTR, 8, "GETPTR" },            { CMD_PROGRESS, 20, "PROGRESS" },
    { CMD_COLDSTART, 4, "COLDSTART" },      { CMD_DIAL, 16, "DIAL" },
    { CMD_LOADIMAGE, 12, "LOADIMAGE" },     { CMD_SNAPSHOT, 8, "SNAPSHOT" },
    { CMD_SCREENSAVER, 4, "SCREENSAVER" },  { CMD_MEMCRC, 16, "MEMCRC" },
    { CMD_SETROTATE, 8, "SETROTATE" },      { CMD_SETFONT2, 16, "SETFONT2" },
    { CMD_SNAPSHOT2, 20, "SNAPSHOT2" },     { CMD_MEDIAFIFO, 12, "MEDIAFIFO" },
    { CMD_INT_SWLOADIMAGE, 12, "INT_SWLOADIMAGE" }, { CMD_SYNC, 4, "SYNC" },
    { CMD_CSKETCH, 20, "CSKETCH" },         { CMD_ROMFONT, 12, "ROMFONT" },
    { CMD_PLAYVIDEO, 8, "PLAYVIDEO" },      { CMD_VIDEOFRAME, 12, "VIDEOFRAME" },
    { CMD_VIDEOSTART, 4, "VIDEOSTART" },    { CMD_SETBASE, 8, "SETBASE" },
    { CMD_SETBITMAP, 16, "SETBITMAP" },     { CMD_SETSCRATCH, 8, "SETSCRATCH" },
};

const CmdInfo* findCmd(uint32_t id)
{
    for (const CmdInfo& c : kCmds)
        if (c.id == id) return &c;
    return nullptr;
}

inline int32_t sext(uint32_t v, int bits) { return (int32_t)(v << (32 - bits)) >> (32 - bits); }
inline int16_t lo16(uint32_t w) { return (int16_t)(w & 0xffff); }
inline int16_t hi16(uint32_t w) { return (int16_t)(w >> 16); }
inline uint32_t fx8(double v) { return (uint32_t)(int32_t)llround(v * 256.0); }

}   // namespace

EveEmu::EveEmu(Canvas& c)
    : canvas(c), ramG(EVE_RAM_G_SIZE, 0), ramDl(EVE_RAM_DL_SIZE, 0), ramCmd(EVE_RAM_CMD_SIZE, 0)
{
}

// ---- memory ------------------------------------------------------------------

uint8_t* EveEmu::mem(uint32_t addr, uint32_t n)
{
    if (addr < EVE_RAM_G_SIZE && n <= EVE_RAM_G_SIZE - addr) return ramG.data() + addr;
    if (addr >= EVE_RAM_DL && addr - EVE_RAM_DL < EVE_RAM_DL_SIZE && n <= EVE_RAM_DL_SIZE - (addr - EVE_RAM_DL))
        return ramDl.data() + (addr - EVE_RAM_DL);
    if (addr >= EVE_RAM_CMD && addr - EVE_RAM_CMD < EVE_RAM_CMD_SIZE && n <= EVE_RAM_CMD_SIZE - (addr - EVE_RAM_CMD))
        return ramCmd.data() + (addr - EVE_RAM_CMD);
    if (!logged[LOG_MEM]) {
        logged[LOG_MEM] = true;
        fprintf(stderr, "EveEmu: access outside RAM_G/RAM_DL/RAM_CMD at 0x%06x (%u bytes), ignored\n", addr, n);
    }
    return nullptr;
}

void EveEmu::memWrite(uint32_t addr, const uint8_t* p, uint32_t n)
{
    if (uint8_t* d = mem(addr, n)) memcpy(d, p, n);
}

void EveEmu::memRead(uint32_t addr, uint8_t* p, uint32_t n)
{
    if (uint8_t* s = mem(addr, n)) memcpy(p, s, n);
    else memset(p, 0, n);
}

// API_Layer.cpp:168-220: bytes 0..143 copied, 144..147 := LE32(addr+148), rest copied.
void EveEmu::fontWrite(uint32_t addr, const uint8_t* p, uint32_t n)
{
    uint8_t* d = mem(addr, n);
    if (!d || n < 148) return;
    memcpy(d, p, 144);
    const uint32_t glyphs = addr + 148;
    d[144] = (uint8_t)glyphs; d[145] = (uint8_t)(glyphs >> 8); d[146] = (uint8_t)(glyphs >> 16); d[147] = (uint8_t)(glyphs >> 24);
    memcpy(d + 148, p + 148, n - 148);
}

uint32_t EveEmu::ring32(uint32_t off) const
{
    uint32_t v = 0;
    for (int i = 3; i >= 0; i--) v = (v << 8) | ramCmd[(off + i) & 4095];
    return v;
}

void EveEmu::ringWrite32(uint32_t off, uint32_t v)
{
    for (int i = 0; i < 4; i++) ramCmd[(off + i) & 4095] = (uint8_t)(v >> (8 * i));
}

// NUL-terminated string at off; paddedBytes = bytes + NUL rounded up to 4 (sendString layout).
std::string EveEmu::ringString(uint32_t off, uint32_t& paddedBytes) const
{
    std::string s;
    while (s.size() < 4095) {
        const uint8_t b = ramCmd[(off + s.size()) & 4095];
        if (!b) break;
        s.push_back((char)b);
    }
    paddedBytes = ((uint32_t)s.size() + 1 + 3) & ~3u;
    return s;
}

void EveEmu::getImgProps(uint32_t& end, uint32_t& w, uint32_t& h)
{
    end = ring32(cmdWrite - 12);
    w = ring32(cmdWrite - 8);
    h = ring32(cmdWrite - 4);
}

// ---- bursts (the driver's coprocessor list) ------------------------------------

void EveEmu::beginBurst()
{
    burstBase = cmdWrite;
    burstLen = 0;
}

void EveEmu::burst32(uint32_t w)
{
    ringWrite32(burstBase + burstLen, w);
    burstLen += 4;
}

uint8_t EveEmu::burstString(const char* s, uint32_t len)
{
    // cList draws rows straight from list->data, which can hold a null row while a file list fills.
    if (!s) s = "";
    const uint32_t real = (uint32_t)strlen(s);
    if (len == 0 || len > real) len = real;
    for (uint32_t i = 0; i < len; i++) ramCmd[(burstBase + burstLen + i) & 4095] = (uint8_t)s[i];
    burstLen += len;
    const uint8_t nul = 0;
    burstBytes(&nul, 1);   // the NUL, then pad to 4
    return (uint8_t)((len + 1 + 3) & ~3u);
}

void EveEmu::burstBytes(const uint8_t* p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) ramCmd[(burstBase + burstLen + i) & 4095] = p[i];
    burstLen += n;
    while (burstLen & 3) ramCmd[(burstBase + burstLen++) & 4095] = 0;
}

void EveEmu::endBurst()
{
    cmdWrite = (burstBase + burstLen) & 4095;
    execFifo();
}

bool EveEmu::takeFrame()
{
    const bool r = frameReady;
    frameReady = false;
    return r;
}

void EveEmu::dlAppend(uint32_t w)
{
    if (dlLen + 4 <= EVE_RAM_DL_SIZE) {
        memcpy(ramDl.data() + dlLen, &w, 4);
        dlLen += 4;
    }
}

void EveEmu::dumpCmd(uint32_t id, uint32_t used, const char* extra)
{
    if (!dump) return;
    const CmdInfo* c = findCmd(id);
    if (c) fprintf(dump, "CMD_%s", c->name);
    else fprintf(dump, "CMD_%08x", id);
    const uint32_t argWords = extra ? 2 : used / 4 - 1;
    for (uint32_t i = 1; i <= argWords; i++) fprintf(dump, " %08x", ring32(cmdRead + 4 * i));
    if (extra) fprintf(dump, " \"%s\"", extra);
    if (id == CMD_SWAP) fprintf(dump, " (displayed %zu words, %zu bytes)", displayed.size(), displayed.size() * 4);
    fputc('\n', dump);
}

// ---- coprocessor executor ---------------------------------------------------------

void EveEmu::execFifo()
{
    while (cmdRead != cmdWrite) {
        if (collectingJpeg) { loadImageFeed(); continue; }
        const uint32_t w = ring32(cmdRead);
        if ((w & kCmdMask) != kCmdMask) {   // display-list word, copied verbatim
            dlAppend(w);
            cmdRead = (cmdRead + 4) & 4095;
            continue;
        }
        auto arg = [&](int i) { return ring32(cmdRead + 4 * i); };
        uint32_t used = 4;
        std::string text;
        const char* extra = nullptr;
        switch (w) {
            case CMD_DLSTART: dlLen = 0; break;   // scissor and colours are re-emitted by the firmware itself
            case CMD_SWAP: {
                displayed.resize(dlLen / 4);
                memcpy(displayed.data(), ramDl.data(), dlLen);
                GfxState st;
                rasterise(displayed.data(), displayed.size(), st);
                frameReady = true;
                lastSwapTime = std::chrono::steady_clock::now();
                lastSwapSimUs = SimClock::us;
                break;
            }
            case CMD_APPEND: {
                const uint32_t ptr = arg(1), num = arg(2);
                const uint32_t n = std::min(num, EVE_RAM_DL_SIZE - dlLen);
                if (const uint8_t* src = mem(ptr, n)) memcpy(ramDl.data() + dlLen, src, n);
                dlLen += n;
                used = 12;
                break;
            }
            case CMD_MEMCPY: {
                const uint32_t dst = arg(1), src = arg(2), num = arg(3);
                uint8_t* d = mem(dst, num);
                const uint8_t* s = mem(src, num);
                if (d && s) memmove(d, s, num);
                used = 16;
                break;
            }
            case CMD_MEMZERO: {
                if (uint8_t* d = mem(arg(1), arg(2))) memset(d, 0, arg(2));
                used = 12;
                break;
            }
            case CMD_TEXT: {
                uint32_t strBytes = 0;
                text = ringString(cmdRead + 12, strBytes);
                expandText(lo16(arg(1)), hi16(arg(1)), (uint16_t)(arg(2) & 0xffff), (uint16_t)(arg(2) >> 16), text);
                used = 12 + strBytes;
                extra = text.c_str();
                break;
            }
            case CMD_NUMBER: {
                // Untested assumption (not checked against the FT81x programmer's guide): the value is
                // unsigned unless OPT_SIGNED, hex digits are upper case.
                const uint16_t opts = (uint16_t)(arg(2) >> 16);
                const int32_t n = (int32_t)arg(3);
                uint32_t u = (uint32_t)n;
                bool neg = false;
                if ((opts & OPT_SIGNED) && n < 0) { neg = true; u = (uint32_t)(-(int64_t)n); }
                const uint32_t base = (numberBase >= 2 && numberBase <= 16) ? numberBase : 10;
                char buf[40];
                int len = 0;
                do { buf[len++] = "0123456789ABCDEF"[u % base]; u /= base; } while (u);
                if (neg) buf[len++] = '-';
                text.assign(len, ' ');
                for (int i = 0; i < len; i++) text[i] = buf[len - 1 - i];
                expandText(lo16(arg(1)), hi16(arg(1)), (uint16_t)(arg(2) & 0xffff), opts, text);
                used = 16;
                break;
            }
            case CMD_SETBASE: numberBase = arg(1); used = 8; break;
            case CMD_GRADIENT:
                dlAppend(PRIV_GRADIENT_OP << 24);
                for (int i = 1; i <= 4; i++) dlAppend(arg(i));
                used = 20;
                break;
            case CMD_SETBITMAP: setBitmap(arg(1), (uint16_t)(arg(2) & 0xffff), (uint16_t)(arg(2) >> 16), (uint16_t)(arg(3) & 0xffff)); used = 16; break;
            case CMD_LOADIDENTITY: { const double I[6] = { 1, 0, 0, 0, 1, 0 }; memcpy(M, I, sizeof M); break; }
            case CMD_TRANSLATE: {   // M = M * T (post-multiply)
                const double tx = (int32_t)arg(1) / 65536.0, ty = (int32_t)arg(2) / 65536.0;
                M[2] += M[0] * tx + M[1] * ty;
                M[5] += M[3] * tx + M[4] * ty;
                used = 12;
                break;
            }
            case CMD_SCALE: {
                const double sx = (int32_t)arg(1) / 65536.0, sy = (int32_t)arg(2) / 65536.0;
                M[0] *= sx; M[1] *= sy; M[3] *= sx; M[4] *= sy;
                used = 12;
                break;
            }
            case CMD_ROTATE: {   // positive = clockwise on a y-down screen: R = [cos -sin; sin cos]
                const double th = 2.0 * M_PI * (int32_t)arg(1) / 65536.0, c = cos(th), s = sin(th);
                const double a = M[0], b = M[1], d = M[3], e = M[4];
                M[0] = a * c + b * s; M[1] = -a * s + b * c;
                M[3] = d * c + e * s; M[4] = -d * s + e * c;
                used = 8;
                break;
            }
            case CMD_SETMATRIX: setMatrix(); break;
            case CMD_SETFONT2: setFont2(arg(1), arg(2), arg(3)); used = 16; break;
            case CMD_LOADIMAGE: jpegDst = arg(1); jpeg.clear(); collectingJpeg = true; used = 12; break;
            case CMD_GETPROPS:
                ringWrite32(cmdRead + 4, lastImgEnd);
                ringWrite32(cmdRead + 8, lastImgW);
                ringWrite32(cmdRead + 12, lastImgH);
                used = 16;
                break;
            case CMD_SNAPSHOT2:
                snapshot2(arg(1), arg(2), lo16(arg(3)), hi16(arg(3)), arg(4) & 0xffff, arg(4) >> 16);
                used = 20;
                break;
            default: {
                const CmdInfo* c = findCmd(w);
                if (c && c->bytes == 0) { uint32_t strBytes = 0; ringString(cmdRead + 16, strBytes); used = 16 + strBytes; }
                else if (c) used = c->bytes;
                const uint8_t slot = (uint8_t)(w & 0xff);
                if (!logged[slot]) {
                    logged[slot] = true;
                    fprintf(stderr, "EveEmu: unimplemented coprocessor command 0x%08x (%s), skipped\n", w, c ? c->name : "unknown");
                }
                break;
            }
        }
        dumpCmd(w, used, extra);
        const uint32_t avail = (cmdWrite - cmdRead) & 4095;
        if (used > avail) {   // malformed stream: never walk past the write pointer
            if (!logged[LOG_RING]) {
                logged[LOG_RING] = true;
                fprintf(stderr, "EveEmu: command 0x%08x needs %u bytes, %u in the FIFO; stream dropped\n", w, used, avail);
            }
            cmdRead = cmdWrite;
        } else {
            cmdRead = (cmdRead + used) & 4095;
        }
    }
}

// ---- coprocessor helpers ------------------------------------------------------------

void EveEmu::expandText(int16_t x0, int16_t y0, uint16_t font, uint16_t opts, const std::string& s)
{
    int handle = font & 31;
    if (font >= 27 && font <= 29) {
        int& sub = romFont[font - 27];
        if (sub < 0) sub = romFontSubstitute(fonts, font);
        handle = sub;
    }
    if (handle < 0 || !fonts[handle].valid) {
        if (!logged[LOG_FONT]) {
            logged[LOG_FONT] = true;
            fprintf(stderr, "EveEmu: CMD_TEXT with unconfigured font %u, text dropped\n", font);
        }
        return;
    }
    const FontDesc& f = fonts[handle];
    int total = 0;
    for (unsigned char c : s) total += f.widths[c & 127];
    int x = x0, y = y0;
    if (opts & OPT_CENTERX) x -= total / 2;
    if (opts & OPT_RIGHTX) x -= total;
    if (opts & OPT_CENTERY) y -= f.height / 2;
    dlAppend(SAVE_CONTEXT());
    dlAppend(BITMAP_HANDLE(handle));
    dlAppend(BEGIN(BITMAPS));
    for (unsigned char uc : s) {
        const int c = uc & 127;
        if (x >= 0 && x < 512 && y >= 0 && y < 512) {
            dlAppend(VERTEX2II(x, y, handle, c));
        } else {
            // shortcut: VERTEX2F in whole pixels, which is right only under VERTEX_FORMAT(0);
            // every firmware frame sets that, upgrade if a frame ever uses another format.
            dlAppend(CELL(c));
            dlAppend(VERTEX2F(x, y));
        }
        x += f.widths[c];
    }
    dlAppend(END());
    dlAppend(RESTORE_CONTEXT());
}

void EveEmu::setFont2(uint32_t handle, uint32_t ptr, uint32_t firstchar)
{
    const uint8_t* block = mem(ptr, 148);
    if (!block) return;
    handle &= 31;
    const FontDesc f = parseMetricBlock(block);
    fonts[handle] = f;
    // cell = character code: glyph addresses wrap in the 22-bit BITMAP_SOURCE space.
    const uint32_t src = (f.pointer - firstchar * f.stride * f.height) & 0x3FFFFFu;
    handles[handle] = { src, f.format, f.stride, f.width, f.height, NEAREST, BORDER, BORDER };
    // The same set-up as DL words, so the handle survives the swap's interpreter pass.
    // Untested assumption: the chip appends these words too (not checked against the programmer's guide).
    dlAppend(BITMAP_HANDLE(handle));
    dlAppend(BITMAP_SOURCE(src));
    dlAppend(BITMAP_LAYOUT(f.format, f.stride, f.height));
    dlAppend(BITMAP_SIZE(NEAREST, BORDER, BORDER, f.width, f.height));
}

void EveEmu::setBitmap(uint32_t src, uint16_t fmt, uint16_t w, uint16_t h)
{
    uint32_t stride;
    switch (fmt) {
        case L1: stride = (w + 7) / 8; break;
        case L2: stride = (w + 3) / 4; break;
        case L4: stride = (w + 1) / 2; break;
        case RGB565: stride = w * 2u; break;
        default: stride = w; break;   // L8, RGB332
    }
    // Untested assumption: the chip emits these five words for CMD_SETBITMAP.
    dlAppend(BITMAP_SOURCE(src));
    dlAppend(BITMAP_LAYOUT(fmt, stride, h));
    dlAppend(BITMAP_LAYOUT_H(stride >> 10, h >> 9));
    dlAppend(BITMAP_SIZE(NEAREST, BORDER, BORDER, w, h));
    dlAppend(BITMAP_SIZE_H(w >> 9, h >> 9));
}

// inverse(M) (screen -> bitmap) as BITMAP_TRANSFORM_A..F, a,b,d,e 8.8 and c,f 15.8.
void EveEmu::setMatrix()
{
    const double a = M[0], b = M[1], c = M[2], d = M[3], e = M[4], f = M[5];
    const double det = a * e - b * d;
    double ia = 1, ib = 0, ic = 0, id = 0, ie = 1, iff = 0;
    if (det != 0.0) {
        ia = e / det; ib = -b / det; ic = (b * f - c * e) / det;
        id = -d / det; ie = a / det; iff = (c * d - a * f) / det;
    }
    dlAppend(BITMAP_TRANSFORM_A(fx8(ia)));
    dlAppend(BITMAP_TRANSFORM_B(fx8(ib)));
    dlAppend(BITMAP_TRANSFORM_C(fx8(ic)));
    dlAppend(BITMAP_TRANSFORM_D(fx8(id)));
    dlAppend(BITMAP_TRANSFORM_E(fx8(ie)));
    dlAppend(BITMAP_TRANSFORM_F(fx8(iff)));
}

void EveEmu::loadImageFeed()
{
    while (cmdRead != cmdWrite) {
        const uint8_t b = ramCmd[cmdRead];
        cmdRead = (cmdRead + 1) & 4095;
        jpeg.push_back(b);
        // shortcut: a flat FF D9 scan; an EXIF thumbnail's own EOI ends collection early,
        // upgrade to a marker-segment walk if covers with thumbnails fail to load.
        if (jpeg.size() >= 2 && jpeg[jpeg.size() - 2] == 0xFF && b == 0xD9) {
            cmdRead = (cmdRead + 3) & ~3u & 4095;   // the writer padded the last chunk to 4
            collectingJpeg = false;
            int w = 0, h = 0, n = 0;
            unsigned char* px = stbi_load_from_memory(jpeg.data(), (int)jpeg.size(), &w, &h, &n, 3);
            uint8_t* dst = px ? mem(jpegDst, (uint32_t)(w * h * 2)) : nullptr;
            if (dst) {
                for (int i = 0; i < w * h; i++) {
                    const uint16_t v = (uint16_t)(((px[3 * i] >> 3) << 11) | ((px[3 * i + 1] >> 2) << 5) | (px[3 * i + 2] >> 3));
                    dst[2 * i] = (uint8_t)v;
                    dst[2 * i + 1] = (uint8_t)(v >> 8);
                }
                lastImgW = (uint32_t)w; lastImgH = (uint32_t)h; lastImgEnd = jpegDst + (uint32_t)(w * h * 2);
            } else {
                lastImgW = lastImgH = 0; lastImgEnd = jpegDst;
                if (!logged[LOG_JPEG]) {
                    logged[LOG_JPEG] = true;
                    fprintf(stderr, "EveEmu: CMD_LOADIMAGE: %s (%zu bytes)\n", px ? "image does not fit RAM_G" : stbi_failure_reason(), jpeg.size());
                }
            }
            if (px) stbi_image_free(px);
            jpeg.clear();
            return;
        }
    }
}

// ARGB8 (0x20): width is given as 2x pixels, rows of B,G,R,A from the displayed frame.
void EveEmu::snapshot2(uint32_t fmt, uint32_t ptr, int x, int y, int w, int h)
{
    if (fmt != 0x20) {
        if (!logged[LOG_SNAPSHOT]) {
            logged[LOG_SNAPSHOT] = true;
            fprintf(stderr, "EveEmu: CMD_SNAPSHOT2 format 0x%x unsupported, ignored\n", fmt);
        }
        return;
    }
    const int pw = w / 2, stride = pw * 4;
    if (uint8_t* dst = mem(ptr, (uint32_t)(stride * h))) canvas.readPixels(x, y, pw, h, dst, stride);
}

// ---- display-list interpreter ---------------------------------------------------------

void EveEmu::vertex(GfxState& st, float x, float y, int handle, int cell, int& phase, float& px, float& py)
{
    const Paint paint { st.rgb, st.alpha, st.blendSrc, st.blendDst, Clip { st.scX, st.scY, st.scW, st.scH } };
    switch (st.prim) {
        case BITMAPS: {
            BitmapDesc d = handles[handle & 31];
            if (!d.width) d.width = 512;    // BITMAP_SIZE: 0 means 512
            if (!d.height) d.height = 512;
            const uint32_t off = (d.source + (uint32_t)cell * d.stride * d.height) & 0x3FFFFFu;
            if (const uint8_t* m = mem(off, (uint32_t)d.stride * d.height))
                canvas.bitmap(d, m, (int)floorf(x), (int)floorf(y), st.t, paint);
            break;
        }
        case FTPOINTS: canvas.disc(x, y, st.pointSize / 16.f, paint); break;
        case LINES:
            if (!phase) { px = x; py = y; phase = 1; }
            else { canvas.segment(px, py, x, y, st.lineWidth / 16.f, paint); phase = 0; }
            break;
        case LINE_STRIP:
            if (phase) canvas.segment(px, py, x, y, st.lineWidth / 16.f, paint);
            px = x; py = y; phase = 1;
            break;
        case RECTS:
            if (!phase) { px = x; py = y; phase = 1; }
            else { canvas.rect(px, py, x, y, st.lineWidth / 16.f, paint); phase = 0; }
            break;
        default: break;   // edge strips are never emitted
    }
}

void EveEmu::rasterise(const uint32_t* words, size_t n, GfxState& st)
{
    GfxState stack[4];
    int sp = 0, phase = 0;
    float px = 0, py = 0;
    for (size_t i = 0; i < n; i++) {
        const uint32_t w = words[i];
        if ((w >> 30) == 1) {   // VERTEX2F: signed 15-bit, units of 1/(1<<vertexFormat) px
            const float div = (float)(1 << st.vertexFormat);
            vertex(st, sext(w >> 15, 15) / div, sext(w, 15) / div, st.handle, st.cell, phase, px, py);
            continue;
        }
        if ((w >> 30) == 2) {   // VERTEX2II: handle and cell only matter inside BITMAPS (trap 13)
            vertex(st, (float)((w >> 21) & 511), (float)((w >> 12) & 511), (w >> 7) & 31, w & 127, phase, px, py);
            continue;
        }
        BitmapDesc& h = handles[st.handle];
        switch (w >> 24) {
            case 0: return;   // DISPLAY
            case 1: h.source = w & 0x3FFFFF; break;
            case 2: st.clearRgb = w & 0xFFFFFF; break;
            case 4: st.rgb = w & 0xFFFFFF; break;
            case 5: st.handle = w & 31; break;
            case 6: st.cell = w & 127; break;
            case 7:
                h.format = (w >> 19) & 31;
                h.stride = (h.stride & ~1023) | ((w >> 9) & 1023);
                h.height = (h.height & ~511) | (w & 511);
                break;
            case 8:
                h.filter = (w >> 20) & 1; h.wrapx = (w >> 19) & 1; h.wrapy = (w >> 18) & 1;
                h.width = (h.width & ~511) | ((w >> 9) & 511);
                h.height = (h.height & ~511) | (w & 511);
                break;
            case 11: st.blendSrc = (w >> 3) & 7; st.blendDst = w & 7; break;
            case 13: st.pointSize = w & 8191; break;
            case 14: st.lineWidth = w & 4095; break;
            case 16: st.alpha = w & 255; break;
            case 21: st.t.a = sext(w, 17); break;
            case 22: st.t.b = sext(w, 17); break;
            case 23: st.t.c = sext(w, 24); break;
            case 24: st.t.d = sext(w, 17); break;
            case 25: st.t.e = sext(w, 17); break;
            case 26: st.t.f = sext(w, 24); break;
            case 27: st.scX = (w >> 11) & 2047; st.scY = w & 2047; break;
            case 28: st.scW = (w >> 12) & 4095; st.scH = w & 4095; break;
            case 31: st.prim = w & 15; phase = 0; break;
            case 33: st.prim = 0; break;
            case 34: if (sp < 4) stack[sp++] = st; break;
            case 35: if (sp > 0) st = stack[--sp]; break;
            case 38: if (w & 4) canvas.clear(st.clearRgb); break;
            case 39: st.vertexFormat = w & 7; break;
            case 40:
                h.stride = (h.stride & 1023) | (((w >> 2) & 3) << 10);
                h.height = (h.height & 511) | ((w & 3) << 9);
                break;
            case 41:
                h.width = (h.width & 511) | (((w >> 2) & 3) << 9);
                h.height = (h.height & 511) | ((w & 3) << 9);
                break;
            case PRIV_GRADIENT_OP: {
                if (i + 4 >= n) return;
                const uint32_t p0 = words[i + 1], c0 = words[i + 2], p1 = words[i + 3], c1 = words[i + 4];
                const Paint paint { st.rgb, st.alpha, st.blendSrc, st.blendDst, Clip { st.scX, st.scY, st.scW, st.scH } };
                canvas.gradient(paint.clip, lo16(p0), hi16(p0), c0, lo16(p1), hi16(p1), c1, paint);
                i += 4;
                break;
            }
            default: break;   // TAG, STENCIL, MACRO, ...: never emitted, ignored
        }
    }
}
