#include "io/Legend.h"

#include "InputMap.h"
#include "eve/EveEmu.h"

#include <cstdio>

// display.cpp loads five fonts at handles i + 1: 1 regular 14 px, 2 regular/light 17 px,
// 3 bold 14, 4 bold 17, 5 regular 12 (Roboto Mono or Liberation Sans by the firmware's
// font setting). Labels and the hint line use 5, key names 2.
static const int kFont = 5, kNameFont = 2, kKeyNameFont = 1, kHintFont = 1;   // row 1 names: 14 px, the cells are narrow
static const uint32_t kBox = 0x1C1C1C, kLabel = 0x8C8C8C, kName = 0xFFFFFF;
static const uint32_t kChordName = 0xFFC040, kHeldOn = 0xE0A000;
static const int kGap = 4, kCellH = 56, kLineH = 24, kRowY[3] = { 4, 64, 124 }, kLabelY = 4, kNameY = 26;

const Legend::Cell Legend::kShoulders[4] = { { "l1", "L1" }, { "l2", "L2" }, { "r2", "R2" }, { "r1", "R1" } }; // physical order
// Labels are the Brick print names (Y is West, X North, B South, A East; f1 is L4, f2 is R4).
const Legend::Cell Legend::kKeys[] = {
    { "dpad", "D-pad" }, { "south", "B" }, { "east", "A" }, { "west", "Y" }, { "north", "X" },
    { "start", "Start" }, { "select", "Select" }, { "menu", "Menu" },
    { "f1", "L4" }, { "f2", "R4" }, { "l3", "L3" }, { "r3", "R3" },
};
static_assert(sizeof(Legend::kKeys) / sizeof(Legend::kKeys[0]) == Legend::kKeyCount, "kKeyCount");

// Chords in Brick keys (Y Shift, A the column key, L4 the chord key), read from the
// firmware's input handlers. Page-independent on purpose.
// shortcut: no page or edit-mode awareness; add a page column if the lines mislead on the device.
const Legend::Modifier Legend::kModifiers[] = {
    { "shift", "west", "Shift held: amber names are its shortcuts" },
    { "column", "east", "Column key held: it is the Note / Instr / FX1 / FX2 key of the cursor column" },
    { "menu", "menu", "Menu held: D-pad picks the page, release opens it, B cycles the page mode" },
    { "mod", "f1", "L4 held: amber names are its shortcuts" },
};
const int Legend::kModifierCount = sizeof(Legend::kModifiers) / sizeof(Legend::kModifiers[0]);
// Shift + a value key is master volume (interfaceInputs.cpp:197-283). Column
// + L4 + L2 pastes into that column only (paste is Shift + Copy, getSelectedElement).
const Legend::Chord Legend::kChords[] = {
    { "shift", "dpad", "select" }, { "shift", "l1", "volume" }, { "shift", "l2", "volume" },
    { "shift", "r2", "volume" }, { "shift", "r1", "volume" },
    { "shift", "north", "backspc" }, { "shift", "east", "keyboard" }, { "shift", "south", "volume" },
    { "shift", "select", "rec arm" }, { "shift", "start", "song" },
    { "column", "north", "del col" }, { "column", "f1", "+L2 paste" },
    { "menu", "dpad", "page" }, { "menu", "south", "next mode" },
    { "mod", "dpad", "Fn 1-4" }, { "mod", "north", "undo" }, { "mod", "south", "redo" }, { "mod", "l1", "copy" }, { "mod", "l2", "paste" },
    { "mod", "r2", "insert" }, { "mod", "r1", "preview" }, { "mod", "menu", "scale" },
};
const int Legend::kChordCount = sizeof(Legend::kChords) / sizeof(Legend::kChords[0]);
const Legend::Page Legend::kPages[] = {
    { "pattern", "Pattern" }, { "song", "Song" }, { "perform", "Perform" }, { "params", "Instrument" },
    { "sample_play", "Sample Playback" }, { "sample_edit", "Sample Editor" }, { "sample_rec", "Sample Recorder" },
    { "sample_load", "Sample Loader" }, { "file", "File" }, { "config", "Config" }, { "master", "Master" },
    { "image", "Image" },
};
const int Legend::kPageCount = sizeof(Legend::kPages) / sizeof(Legend::kPages[0]);
static const char* const kIdleHint = "hold Y, A, L4 or Menu to see its shortcuts   R4: action list   Select+Start 2 s: quit";
static const char* const kListHint = "List: Left/Right pick, L4 next page, shoulders nudge, Enter runs, R4 closes";
// Cell names for Mini ids that read badly as they are; every other id shows itself.
static const struct { const char* id; const char* name; } kNames[] = {
    { "minus1", "-1" }, { "minus10", "-10" }, { "plus10", "+10" }, { "plus1", "+1" }, { "mod", "edit" },
};

const char* Legend::displayName(const char* id)
{
    for (const auto& n : kNames)
        if (!strcmp(n.id, id)) return n.name;
    return id;
}

const char* Legend::pageTitle(const char* id)
{
    for (int i = 0; id && i < kPageCount; i++)
        if (!strcmp(kPages[i].id, id)) return kPages[i].title;
    return "";
}

const Legend::Modifier* Legend::activeModifier(const char* const held[], int n)
{
    for (int m = 0; m < kModifierCount; m++)
        for (int i = 0; i < n; i++)
            if (!strcmp(held[i], kModifiers[m].input)) return &kModifiers[m];
    return nullptr;
}

const char* Legend::chord(const char* modifier, const char* input)
{
    for (int i = 0; i < kChordCount; i++)
        if (!strcmp(kChords[i].modifier, modifier) && !strcmp(kChords[i].input, input)) return kChords[i].name;
    return nullptr;
}

Clip Legend::shoulderRect(int k) { return Clip{ k * (W / 4) + kGap / 2, kRowY[0], W / 4 - kGap, kCellH }; }

bool Legend::keyRect(const char* input, Clip& out, int count)
{
    const int unit = W / (count + 1);   // the D-pad cell is two units wide
    int x = 0;
    for (int c = 0; c < count; c++)
    {
        const int w = !strcmp(kKeys[c].input, "dpad") ? 2 * unit : unit;
        if (!strcmp(kKeys[c].input, input))
        {
            out = Clip{ x + kGap / 2, kRowY[1], w - kGap, kCellH };
            return true;
        }
        x += w;
    }
    return false;
}

Clip Legend::hintRect() { return Clip{ kGap / 2, kRowY[2], W - kGap, kLineH }; }

void Legend::draw(EveEmu& emu, const char* const shoulders[4], const char* const keys[], const char* pageId,
                  const char* const heldIn[], int heldCount, bool modHeld, bool listVisible)
{
    cv.clear(0);
    if (!emu.font(kFont).valid) return;     // fonts arrive with the firmware's first frames
    const char* held[33];
    for (int i = 0; i < heldCount; i++) held[i] = heldIn[i];
    if (modHeld) held[heldCount++] = InputMap::kModInput;
    Paint box{ kBox, 255, BF_ONE, BF_ZERO, Clip{ 0, 0, W, H } };
    auto cell = [&](const Clip& r, uint32_t bg, const char* label, uint32_t labelCol, const char* name, uint32_t nameCol, int nameFont) {
        box.rgb = bg;
        cv.rect(r.x, r.y, r.x + r.w, r.y + r.h, 0, box);
        const Clip in{ r.x + 2, r.y, r.w - 4, r.h };
        if (label) text(emu, kFont, r.x + 4, r.y + kLabelY, label, labelCol, in);
        if (name) text(emu, nameFont, r.x + 4, r.y + kNameY, name, nameCol, in);
    };
    const Modifier* mod = activeModifier(held, heldCount);
    // A cell: amber while it is the held modifier, else its chord name in amber or its key in white.
    // The plain Brick prints L3 / R3 on its two under-screen buttons (the Pro's L4 / R4).
    const bool plain = keyCount < kKeyCount;
    auto keyCell = [&](const Clip& r, const Cell& k, const char* base, int nameFont) {
        const char* label = !plain ? k.label : !strcmp(k.input, "f1") ? "L3" : !strcmp(k.input, "f2") ? "R3" : k.label;
        if (mod && !strcmp(mod->input, k.input)) return cell(r, kHeldOn, label, 0, base, 0, nameFont);
        const char* c = mod ? chord(mod->id, k.input) : nullptr;
        cell(r, kBox, label, kLabel, c ? c : base, c ? kChordName : kName, nameFont);
    };

    for (int k = 0; k < 4; k++) keyCell(shoulderRect(k), kShoulders[k], shoulders[k], kNameFont);   // row 0
    for (int c = 0; c < keyCount; c++)                                                               // row 1
    {
        Clip r;
        keyRect(kKeys[c].input, r, keyCount);
        keyCell(r, kKeys[c], keys[c], kKeyNameFont);
    }

    // row 2: page and hint
    const Clip f = hintRect();
    box.rgb = kBox;
    cv.rect(f.x, f.y, f.x + f.w, f.y + f.h, 0, box);
    char line[256];
    const char* title = pageTitle(pageId);
    const char* hint = listVisible ? kListHint : mod ? mod->hint : kIdleHint;
    snprintf(line, sizeof(line), "%s%s   %s", title, title[0] ? " page" : "", hint);
    if (plain)   // the hints name the under-screen buttons as the Pro prints them
        for (char* p = line; p[0] && p[1]; p++)
            if ((p[0] == 'L' || p[0] == 'R') && p[1] == '4') p[1] = '3';
    text(emu, kHintFont, f.x + 4, f.y + 3, line, hint == kIdleHint ? kLabel : kChordName, Clip{ f.x + 2, f.y, f.w - 4, f.h });
}

// The glyph blit EveEmu::expandText does through the display list, done directly: the
// font's first glyph is the first code with a non-zero width (display.cpp first_char),
// glyphs follow contiguously, each stride * height bytes of L4.
void Legend::text(EveEmu& emu, int handle, int x, int y, const char* s, uint32_t rgb, const Clip& clip)
{
    const FontDesc& f = emu.font(emu.font(handle).valid ? handle : kFont);
    int first = 0;
    while (first < 128 && !f.widths[first]) first++;
    const uint32_t bytes = (uint32_t)f.stride * f.height;
    glyph.resize(bytes);
    const BitmapDesc bm{ 0, f.format, f.stride, f.width, f.height, 0, 0, 0 };
    const Transform id{ 256, 0, 0, 0, 256, 0 };
    const Paint p{ rgb, 255, BF_SRC_ALPHA, BF_ONE_MINUS_SRC_ALPHA, clip };
    for (; *s; s++)
    {
        const int c = *s & 127;
        if (!f.widths[c]) continue;
        emu.memRead(f.pointer + (uint32_t)(c - first) * bytes, glyph.data(), bytes);
        cv.bitmap(bm, glyph.data(), x, y, id, p);
        x += f.widths[c];
    }
}
