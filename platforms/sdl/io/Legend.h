// Legend strip v3: row 0 sits under the firmware's label bar and
// shows the four shoulders as value keys (-1 -10 +10 +1), row 1 one cell per other key with
// the Mini key it sends, or, while a modifier (Shift, the column key, Menu, L4) is held, what
// each key does in that chord, row 2 one line: the page and a hint (the list's keys while it
// is up). Drawn with the firmware's fonts straight out of the emulator's RAM_G.
#pragma once

#include "eve/Canvas.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

class EveEmu;

class Legend
{
public:
    static constexpr int W = 1024, H = 154;   // SdlDisplayOut's strip texture size

    Legend() : cv(W, H) { cv.clear(0); }

    // rows: any table with const char* input and base (InputMap::kBrickBindings), Mini string
    // ids, "" unbound. modHeld: L4 is down (SdlInput::modHeld). page: PageId.h id.
    // heldMask: bit i set while rows[i] is physically down (modifier detection).
    // listVisible: the action list is up (PageId listVisible).
    template <class Row>
    void render(EveEmu& emu, const Row* rows, size_t count, bool modHeld, const char* page, uint32_t heldMask, bool listVisible)
    {
        const char* shoulders[4];
        const char* keys[kKeyCount];
        for (int k = 0; k < 4; k++) shoulders[k] = displayName(base(rows, count, kShoulders[k].input));
        for (int c = 0; c < kKeyCount; c++)
            keys[c] = !strcmp(kKeys[c].input, "dpad") ? "move" : displayName(base(rows, count, kKeys[c].input));
        const char* held[32];
        int n = 0;
        for (size_t i = 0; i < count && i < 32; i++)
            if (heldMask & (1u << i)) held[n++] = rows[i].input;
        draw(emu, shoulders, keys, page, held, n, modHeld, listVisible);
    }

    const Canvas& canvas() const { return cv; }
    const uint32_t* pixels() const { return cv.pixels(); }

    static Clip shoulderRect(int k);                // kShoulders[k] cell, row 0
    static bool keyRect(const char* input, Clip& out, int count = kKeyCount);   // row 1 cell of `count` cells; false if none
    static Clip hintRect();                         // row 2

    struct Cell { const char* input; const char* label; };
    static const Cell kShoulders[4];   // L1 L2 R2 R1: the Brick's physical order
    static const Cell kKeys[];         // row 1; "dpad" stands for the four D-pad inputs
    static const int kKeyCount = 12;   // the last two are L3 and R3 (Brick Pro sticks)
    // false on the plain Brick: no L3 / R3 cells, the other ten share the row
    void setStickKeys(bool on) { keyCount = on ? kKeyCount : kKeyCount - 2; }
    int keyCells() const { return keyCount; }
    // A held modifier and the names it gives the other keys.
    struct Modifier { const char* id; const char* input; const char* hint; };
    struct Chord { const char* modifier; const char* input; const char* name; };
    static const Modifier kModifiers[];
    static const int kModifierCount;
    static const Chord kChords[];
    static const int kChordCount;
    struct Page { const char* id; const char* title; };
    static const Page kPages[];
    static const int kPageCount;
    static const char* pageTitle(const char* id);   // "" when unknown
    // The first modifier in kModifiers whose input is held, nullptr when none.
    static const Modifier* activeModifier(const char* const held[], int n);
    static const char* chord(const char* modifier, const char* input);   // nullptr when none
    static const char* displayName(const char* id);   // what a cell shows for a Mini id ("minus1" is "-1")

private:
    int keyCount = kKeyCount;
    template <class Row>
    static const char* base(const Row* rows, size_t count, const char* input)
    {
        for (size_t i = 0; i < count; i++)
            if (!strcmp(rows[i].input, input)) return rows[i].base;
        return "";
    }
    void draw(EveEmu& emu, const char* const shoulders[4], const char* const keys[], const char* page,
              const char* const held[], int heldCount, bool modHeld, bool listVisible);
    void text(EveEmu& emu, int handle, int x, int y, const char* s, uint32_t rgb, const Clip& clip);

    Canvas cv;
    std::vector<uint8_t> glyph;
};
