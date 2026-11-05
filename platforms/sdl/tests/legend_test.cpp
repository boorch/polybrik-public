// Assert-based self-check of the legend strip (io/Legend v3): cell layout, colours, the
// held-modifier chords (Shift, L4), the list hint, and the all-black strip before fonts arrive.
// argv[1], argv[2], argv[3], when given, are PNG paths for the strip idle, Shift held, L4 held.
#undef NDEBUG
#include "FT81x.h"
#include "NewDisplay/CompressionRLE.h"
#include "RobotoMono-Light_17_L4.h"
#include "RobotoMono-Regular_12_L4.h"
#include "RobotoMono-Regular_14_L4.h"
#include "eve/EveDisplayDriver.h"
#include "eve/EveEmu.h"
#include "io/Legend.h"
#include "io/Png.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

// Stand-ins for interfaceMenuPopup.cpp's list (same names), so the test does not pull
// the module objects out of tracker_core.
extern const uint8_t kMenuModulesCount;
extern const char* const kModuleNamesList[];
const uint8_t kMenuModulesCount = 11;
const char* const kModuleNamesList[] = { "Pattern", "Instrument Parameters", "Sample Playback", "Master",
                                         "Song", "Sample Loader", "File", "Config", "Perform",
                                         "Sample Editor", "Sample Recorder" };

struct Row { const char* input; const char* base; const char* mod; };
// Stub table: a few rows of INPUT-MAP.md, l3/r3 left out on purpose (cells stay empty).
static const Row kRows[] = {
    { "l1", "minus1", "copy" }, { "l2", "minus10", "paste" }, { "r2", "plus10", "insert" }, { "r1", "plus1", "preview" },
    { "dpad_up", "up", "" }, { "south", "enter", "" }, { "east", "column", "" },
    { "west", "shift", "shift" }, { "north", "delete", "" },
    { "start", "play", "play" }, { "select", "rec", "rec" }, { "menu", "menu", "" },
    { "f2", "list", "list" }, { "f1", "mod", "mod" },
};
static uint32_t px(const Legend& l, int x, int y) { return l.pixels()[y * Legend::W + x] & 0xFFFFFF; }

static int litIn(const Legend& l, const Clip& r)   // pixels brighter than the box colour (green channel: amber text has little blue)
{
    int n = 0;
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            if (((px(l, x, y) >> 8) & 0xFF) > 0x40) n++;
    return n;
}

static bool disjoint(const Clip& a, const Clip& b)
{
    return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
}

// The firmware's loadFont for one font (display.cpp:236-286) at a given handle.
static void uploadFont(EveDisplayDriver& drv, int handle, const CompressedData* data, size_t n, uint32_t addr)
{
    static uint8_t buf[22396];   // display.cpp kMaxFontBufferSize
    decompress(data, n, buf);
    int chars = 0;
    for (int c = 0; c < 128; c++) chars += buf[c] != 0;
    const uint32_t size = chars * buf[132] * buf[140] + 148;
    drv.writeFontRAMG(buf, size, addr);
    uint32_t first = 0;
    while (!buf[first]) first++;
    drv.beginCoProList();
    drv.WriteData32(CMD_SETFONT2); drv.WriteData32(handle); drv.WriteData32(addr); drv.WriteData32(first);
    drv.endCoProList();
}

int main(int argc, char** argv)
{
    Canvas screen(800, 480);
    EveEmu emu(screen);
    EveDisplayDriver drv(emu);
    Legend legend;
    const size_t n = sizeof(kRows) / sizeof(kRows[0]);
    const uint32_t west = 1u << 7, south = 1u << 5, f1Bit = 1u << 13;   // kRows indices

    // ---- fonts not uploaded: all black, no crash ----
    legend.render(emu, kRows, n, false, "pattern", 0, false);
    for (int i = 0; i < Legend::W * Legend::H; i++) assert((legend.pixels()[i] & 0xFFFFFF) == 0);
    printf("no fonts: strip all black\n");

    uploadFont(drv, 5, Roboto_Mono_Regular_12_L4_compressed, sizeof(Roboto_Mono_Regular_12_L4_compressed) / sizeof(CompressedData), 4);
    assert(emu.font(5).valid && emu.font(5).height == 16);
    legend.render(emu, kRows, n, false, "pattern", 0, false);   // only font 5: names fall back to it, no crash
    uploadFont(drv, 1, Roboto_Mono_Regular_14_L4_compressed, sizeof(Roboto_Mono_Regular_14_L4_compressed) / sizeof(CompressedData), 40000);
    uploadFont(drv, 2, Roboto_Mono_Light_17_L4_compressed, sizeof(Roboto_Mono_Light_17_L4_compressed) / sizeof(CompressedData), 80000);
    assert(emu.font(1).valid && emu.font(2).valid && emu.font(2).height > emu.font(5).height);
    printf("fonts 5, 1, 2: %u, %u, %u px tall\n", emu.font(5).height, emu.font(1).height, emu.font(2).height);

    // ---- geometry: four shoulder cells across row 0, key cells in row 1, no overlaps ----
    for (int k = 0; k < 4; k++)
    {
        const Clip r = Legend::shoulderRect(k);
        assert(r.x >= 0 && r.x + r.w <= Legend::W && r.y >= 0 && r.y + r.h <= Legend::H);
        assert(r.w > Legend::W / 4 - 10 && r.w <= Legend::W / 4);   // a quarter each, no 8-slot row
        for (int j = 0; j < k; j++) assert(disjoint(r, Legend::shoulderRect(j)) && Legend::shoulderRect(j).x < r.x);
    }
    for (int c = 0; c < Legend::kKeyCount; c++)
    {
        Clip a;
        assert(Legend::keyRect(Legend::kKeys[c].input, a));
        assert(a.x >= 0 && a.y >= 0 && a.x + a.w <= Legend::W && a.y + a.h <= Legend::H);
        assert(disjoint(a, Legend::shoulderRect(0)) && disjoint(a, Legend::hintRect()));
        for (int d = 0; d < c; d++)
        {
            Clip b;
            Legend::keyRect(Legend::kKeys[d].input, b);
            assert(disjoint(a, b));
        }
    }
    static const char* const kKeyOrder[] = { "dpad", "south", "east", "west", "north", "start", "select", "menu", "f1", "f2", "l3", "r3" };
    for (int c = 0; c < Legend::kKeyCount; c++) assert(!strcmp(Legend::kKeys[c].input, kKeyOrder[c]));
    const Clip f = Legend::hintRect();
    assert(f.x >= 0 && f.y + f.h <= Legend::H);
    Clip none;
    assert(!Legend::keyRect("l2", none) && !Legend::keyRect("nope", none));
    assert(!strcmp(Legend::pageTitle("pattern"), "Pattern") && !strcmp(Legend::pageTitle("nope"), "") && !strcmp(Legend::pageTitle(nullptr), ""));

    // ---- names and chords ----
    assert(!strcmp(Legend::displayName("minus1"), "-1") && !strcmp(Legend::displayName("minus10"), "-10"));
    assert(!strcmp(Legend::displayName("plus10"), "+10") && !strcmp(Legend::displayName("plus1"), "+1"));
    {   // plain Brick: ten cells, no L3 / R3, the row still spans the strip
        Clip a, b;
        assert(Legend::keyRect("r3", a) && !Legend::keyRect("l3", b, Legend::kKeyCount - 2));
        assert(Legend::keyRect("f2", b, Legend::kKeyCount - 2) && b.x + b.w > a.x);   // R4 widened into the freed space
        Legend l; l.setStickKeys(false); assert(l.keyCells() == 10); l.setStickKeys(true); assert(l.keyCells() == 12);
    }
    assert(!strcmp(Legend::displayName("mod"), "edit") && !strcmp(Legend::displayName("list"), "list"));
    assert(!strcmp(Legend::displayName("enter"), "enter") && !strcmp(Legend::displayName(""), ""));
    assert(!strcmp(Legend::chord("mod", "dpad"), "Fn 1-4") && !strcmp(Legend::chord("mod", "l1"), "copy"));
    assert(!strcmp(Legend::chord("mod", "north"), "undo") && !strcmp(Legend::chord("mod", "south"), "redo"));
    assert(!strcmp(Legend::chord("mod", "l2"), "paste") && !strcmp(Legend::chord("mod", "r2"), "insert") && !strcmp(Legend::chord("mod", "r1"), "preview"));
    assert(!strcmp(Legend::chord("shift", "dpad"), "select") && !Legend::chord("shift", "slot") && !Legend::chord("shift", "nope") && !Legend::chord("nope", "dpad"));
    for (int i = 0; i < Legend::kChordCount; i++)
        assert(strcmp(Legend::kChords[i].modifier, "pattern") && strcmp(Legend::kChords[i].modifier, "value"));
    const char* heldWest[] = { "west" };
    const char* heldF1[] = { "f1" };
    const char* heldR1[] = { "r1" };
    const char* heldSouth[] = { "south" };
    assert(Legend::activeModifier(heldWest, 1) && !strcmp(Legend::activeModifier(heldWest, 1)->id, "shift"));
    assert(Legend::activeModifier(heldF1, 1) && !strcmp(Legend::activeModifier(heldF1, 1)->id, "mod"));
    assert(!Legend::activeModifier(heldR1, 1) && !Legend::activeModifier(heldSouth, 1));   // no Pattern key, no B layer
    assert(!Legend::activeModifier(heldWest, 0));
    const int top = 24;   // label band; the name sits below it

    // ---- nothing held: four shoulder cells, the key cells, the page line ----
    legend.render(emu, kRows, n, false, "pattern", 0, false);
    if (argc > 1) assert(savePng(legend.canvas(), argv[1]));
    for (int k = 0; k < 4; k++)
    {
        const Clip r = Legend::shoulderRect(k);
        assert(px(legend, r.x + r.w / 2, r.y + r.h - 1) != 0);   // the box is drawn
        assert(px(legend, r.x - 1, r.y + r.h / 2) == 0);         // black gap before the cell
        assert(px(legend, r.x + r.w, r.y + r.h / 2) == 0);       // and after it
        assert(px(legend, r.x + r.w / 2, r.y - 1) == 0);         // and above it
        assert(litIn(legend, Clip{ r.x, r.y, r.w, top }) > 10);                       // "L1"
        assert(litIn(legend, Clip{ r.x, r.y + top, r.w, r.h - top }) > 10);           // "-1"
    }
    for (int c = 0; c < Legend::kKeyCount; c++)
    {
        Clip r;
        Legend::keyRect(Legend::kKeys[c].input, r);
        assert(px(legend, r.x + r.w / 2, r.y + r.h - 1) != 0);
        assert(px(legend, r.x - 1, r.y + r.h / 2) == 0 && px(legend, r.x + r.w, r.y + r.h / 2) == 0);
        assert(litIn(legend, Clip{ r.x, r.y, r.w, top }) > 10);   // physical label
        const bool unbound = !strcmp(Legend::kKeys[c].input, "l3") || !strcmp(Legend::kKeys[c].input, "r3");
        assert(unbound ? litIn(legend, Clip{ r.x, r.y + top, r.w, r.h - top }) == 0
                       : litIn(legend, Clip{ r.x, r.y + top, r.w, r.h - top }) > 10);
    }
    assert(litIn(legend, f) > 300);   // "Pattern" plus the idle hint
    for (int x = 0; x < Legend::W; x++) assert(px(legend, x, Legend::H - 1) == 0);   // bottom rows unused
    Clip f1, f2, dpad, westCell;
    Legend::keyRect("f1", f1);
    Legend::keyRect("f2", f2);
    Legend::keyRect("dpad", dpad);
    Legend::keyRect("west", westCell);
    assert(px(legend, f1.x + f1.w - 3, f1.y + 3) == 0x1C1C1C);
    std::vector<uint32_t> idle(legend.pixels(), legend.pixels() + Legend::W * Legend::H);
    auto changedIn = [&](const Clip& r) {
        int c = 0;
        for (int y = r.y; y < r.y + r.h; y++)
            for (int x = r.x; x < r.x + r.w; x++) c += (legend.pixels()[y * Legend::W + x] != idle[y * Legend::W + x]);
        return c;
    };
    printf("idle: shoulders, keys, hint line ok\n");

    // ---- unknown page: hint only ----
    legend.render(emu, kRows, n, false, "nope", 0, false);
    assert(litIn(legend, f) > 100);

    // ---- Shift held (west): its cell amber, the D-pad cell says select, the hint changes ----
    legend.render(emu, kRows, n, false, "pattern", west, false);
    if (argc > 2) assert(savePng(legend.canvas(), argv[2]));
    assert(px(legend, westCell.x + westCell.w - 3, westCell.y + 3) == 0xE0A000);
    assert(changedIn(Clip{ dpad.x, dpad.y + top, dpad.w, dpad.h - top }) > 10);   // "move" became "select"
    assert(changedIn(f) > 10);
    printf("shift held: west amber, D-pad renamed\n");

    // ---- L4 held through the mask: f1 amber, shoulders show copy paste insert preview ----
    legend.render(emu, kRows, n, false, "pattern", f1Bit, false);
    assert(px(legend, f1.x + f1.w - 3, f1.y + 3) == 0xE0A000 && px(legend, f2.x + f2.w - 3, f2.y + 3) == 0x1C1C1C);
    for (int k = 0; k < 4; k++)
    {
        const Clip r = Legend::shoulderRect(k);
        assert(changedIn(Clip{ r.x, r.y + top, r.w, r.h - top }) > 10);   // the chord name replaced -1 / -10 / +10 / +1
        assert(changedIn(Clip{ r.x, r.y, r.w, top }) == 0);                // the L1 L2 R2 R1 labels stay
    }
    assert(changedIn(Clip{ dpad.x, dpad.y + top, dpad.w, dpad.h - top }) > 10);   // "undo / redo"
    std::vector<uint32_t> modMask(legend.pixels(), legend.pixels() + Legend::W * Legend::H);

    // ---- L4 held through modHeld alone: the same strip ----
    legend.render(emu, kRows, n, true, "pattern", 0, false);
    if (argc > 3) assert(savePng(legend.canvas(), argv[3]));
    assert(!memcmp(legend.pixels(), modMask.data(), modMask.size() * 4));
    printf("L4 held: f1 amber, shoulders and D-pad renamed\n");

    // ---- B and R1 are plain keys now; an unknown held key: nothing ----
    legend.render(emu, kRows, n, false, "pattern", south, false);
    assert(!memcmp(legend.pixels(), idle.data(), idle.size() * 4));
    legend.render(emu, kRows, n, false, "pattern", 1u << 3, false);   // r1
    assert(!memcmp(legend.pixels(), idle.data(), idle.size() * 4));
    legend.render(emu, kRows, n, false, "pattern", 1u << 9, false);   // start: no modifier
    assert(!memcmp(legend.pixels(), idle.data(), idle.size() * 4));

    // ---- list up: the hint line changes, the cells do not; it wins over a held modifier ----
    legend.render(emu, kRows, n, false, "pattern", 0, true);
    assert(changedIn(f) > 10);
    assert(changedIn(Clip{ 0, 0, Legend::W, f.y }) == 0);
    std::vector<uint32_t> list(legend.pixels() + f.y * Legend::W, legend.pixels() + (f.y + f.h) * Legend::W);
    legend.render(emu, kRows, n, false, "pattern", west, true);
    assert(!memcmp(legend.pixels() + f.y * Legend::W, list.data(), list.size() * 4));
    printf("list up: list hint\n");

    legend.render(emu, kRows, n, false, nullptr, 0, false);   // null page: no crash
    printf("legend_test: all checks passed\n");
    return 0;
}
