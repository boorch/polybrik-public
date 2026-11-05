#ifndef INTERFACE_CORE_SDLACTIONLIST_H_
#define INTERFACE_CORE_SDLACTIONLIST_H_

#ifdef PLATFORM_SDL

// Per-page action list for the SDL port (core patch 24). The list draws into
// the label bar while it is up and owns every key except Play, Rec and Menu.
// Pages without a list get the screen-key mode on the same key (patch 31): a frame over one of
// the firmware's own eight bar labels, Left/Right move it, Enter (B) is that screen key with its
// press, hold and release; every other key reaches the page as usual.

#include <stddef.h>
#include <stdint.h>

struct SdlActionEntry
{
    const char* id;    // stable string id
    const char* label; // cell line 1, at most 10 characters
    enum Kind : uint8_t
    {
        Action,
        Value
    } kind;
    void (*run)();                      // Action: called on momentary release or sticky Enter
    void (*nudge)(int16_t delta);       // Value: delta in {-10, -1, +1, +10}
    void (*text2)(char* out, size_t n); // line 2 (value or state), nullptr = none
    bool (*enabled)();                  // nullptr = always enabled
};

struct SdlActionPage
{
    const SdlActionEntry* entries;
    uint8_t count; // 1..8
};

struct SdlActionListDef
{
    const char* id;
    const SdlActionPage* pages;
    uint8_t pageCount;
};

class SdlActionList
{
  public:
    static constexpr uint32_t kAmber[4] = { 0xFFC040, 0x3A2A00, 0xFFC040, 0 };
    static constexpr uint32_t kBlue[4] = { 0x40C8FF, 0x002A3A, 0x40C8FF, 0 };
    static constexpr uint32_t kDim[4] = { 0x555555, 0x000000, 0x000000, 0 };
    // closed list (text, background, frame, value): the page's cells stay visible, tinted dark
    static constexpr uint32_t kIdle[4] = { 0x4A4A4A, 0x000000, 0x262626, 0x7A7A7A };
    static constexpr uint32_t kIdleDim[4] = { 0x2C2C2C, 0x000000, 0x262626, 0x3A3A3A };

    void initDisplayControls();                 // 8 cLabel cells, hidden; safe to skip (headless)
    void setList(const SdlActionListDef* def, bool keepOpen = false); // nullptr = none; closes the list if visible unless keepOpen
    const SdlActionListDef* list() const { return def; }
    // Module switch: no list, and the page no longer owns the screen-key bar (patch 25).
    void pageChanged();
    // True once the current page has set a list; a two-button window clearing the list does not reset it.
    bool ownsBar() const { return barOwned; }
    void setGenericOnly(bool v) { genericOnly = v; } // set once at boot, before any page opens
    bool handleButton(uint8_t n, uint8_t state); // true = consumed
    bool isVisible() const { return visible; }
    bool isKeysMode() const { return visible && keysMode; }
    bool isSticky() const { return sticky; }
    uint8_t page() const { return pageIdx; }
    uint8_t highlight() const { return hl; } // entry index on the page
    const char* cellLine2(uint8_t cell) const { return cellText2[cell]; } // as last drawn
    void refresh();                          // redraw cells (call after a value changed)
    void idleTick(uint32_t nowMs);           // closed list: repaint the dark cells when a value changed
    static uint32_t fitLine2(char* t, size_t n, uint8_t page, uint8_t pageCount); // returns line 2's font style

  private:
    static constexpr uint8_t kCells = 8;
    static constexpr uint8_t kMemorySlots = 96; // 53 lists exist with the page lists; room to grow

    struct Memory
    {
        const SdlActionListDef* def;
        uint8_t page, hl;
    };

    void open();
    void close();
    bool isEnabled(uint8_t p, uint8_t h) const;
    void seekEnabled(int8_t dir, bool stepFirst);
    void runHighlighted();
    void nudge(int16_t delta);
    const SdlActionEntry& current() const { return def->pages[pageIdx].entries[hl]; }
    Memory* memoryFor(const SdlActionListDef* d);

    bool handleKeysMode(uint8_t n, uint8_t state);
    void refreshKeysFrame();

    const SdlActionListDef* def = nullptr;
    bool visible = false;
    bool keysMode = false;  // visible without a list: the screen-key frame
    uint8_t keysHl = 0;     // screen key under the frame, 0..7
    int8_t forwardKey = -1; // screen key that Enter is holding down (its hold and release follow)
    bool barOwned = false;
    bool sticky = false;
    bool momentary = false;
    void hideCells();
    uint32_t lastIdleMs = 0;
    char idleSig[8 * 40 + 64] = "";
    bool genericOnly = false;   // --generic-r4: curated lists ignored, screen-key mode everywhere
    bool ignoreListKey = false; // swallow the hold/release after the press that closed a sticky list
    bool ignoreColumnKey = false; // swallow A's hold/release after A cancelled a held screen-key frame (patch 31)
    uint64_t owned = 0; // keys whose press the list consumed: their hold and release stay with the list
    uint8_t pageIdx = 0;
    uint8_t hl = 0;
    Memory memory[kMemorySlots] = {};

    int8_t labels[kCells] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    uint32_t cellColors[kCells][4] = {};
    char cellText2[kCells][32] = {}; // full firmware names before fitLine2
};

extern SdlActionList sdlActionList;

#endif // PLATFORM_SDL

#endif // INTERFACE_CORE_SDLACTIONLIST_H_
