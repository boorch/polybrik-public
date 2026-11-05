#ifdef PLATFORM_SDL

#include "core/sdlActionList.h"
#include "core/interface.h"
#include "core/interfacePopups.h"
#include "core/interfaceDefs.h"
#include "display.h"

#include <stdio.h>
#include <string.h>

SdlActionList sdlActionList;

void SdlActionList::initDisplayControls()
{
    strControlProperties prop2;

    for (uint8_t i = 0; i < kCells; i++)
    {
        prop2.text = (char*)"";
        prop2.colors = display.getTheme().activeLabelsColors;

        prop2.style = (controlStyleCenterX | controlStyleCenterY | controlStyleFont3 | controlStyleBorder);
        prop2.x = (800 / 8) * i + (800 / 16);
        prop2.w = 800 / 8 - 6;
        prop2.y = 425; // one pixel off the firmware bar so its label suppression leaves these alone
        prop2.h = 55;

        labels[i] = display.createControl<cLabel>(&prop2);
    }
}

void SdlActionList::setList(const SdlActionListDef* d, bool keepOpen)
{
    // keepOpen: a page swapping its own list while the user drives it (Sample playback play mode) keeps the
    // list up in the same mode, on the first entry of the new list
    const bool reopen = keepOpen && visible && !keysMode;
    const bool wasSticky = sticky, wasMomentary = momentary;
    close();
    ignoreListKey = false;
    def = genericOnly ? nullptr : d; // --generic-r4: every page takes the screen-key mode (patch 31)
    if (def) barOwned = true;
    if (reopen && def)
    {
        Memory* m = memoryFor(def);
        m->page = m->hl = 0;
        open();
        sticky = wasSticky;
        momentary = wasMomentary;
        refresh();
    }
    else if (def)
    {
        Memory* m = memoryFor(def);   // the closed list shows the page the user left it on
        pageIdx = m->page < def->pageCount ? m->page : 0;
        idleSig[0] = 0;
        refresh();
    }
    else
        hideCells();
}

void SdlActionList::pageChanged()
{
    setList(nullptr);
    barOwned = false;
}

bool sdlLabelBarHidden()
{
    // The note keyboard popup draws its eight key labels in the same strip (interfacePopups.cpp): keep them.
    return sdlActionList.ownsBar() && !mtPopups.isNoteKeyboardPopupActive();
}

bool SdlActionList::handleButton(uint8_t n, uint8_t state)
{
    // Modules only ever see whole press/hold/release sequences: a key the list took on press keeps
    // its hold and release (even after the list closed or the module changed), and a key pressed
    // before the list opened still gets its release through.
    static_assert(interfaceButtonsCount <= 64, "owned is a 64-bit mask");
    // Enter pressed in the screen-key mode holds that screen key: hold and release follow it, even
    // after the frame closed, so the page always sees a whole press/hold/release sequence.
    if (n == interfaceButtonMiniMiddle && forwardKey >= 0 && state != buttonPress)
    {
        mtInterface.ip->processButtonsInput((uint8_t)forwardKey, state);
        if (state == buttonRelease) forwardKey = -1;
        return true;
    }
    const uint64_t bit = 1ull << n;
    if (n != interfaceButtonSdlList && (owned & bit))
    {
        if (state == buttonRelease)
        {
            owned &= ~bit;
            return true;
        }
        if (!visible) return true;
    }

    if (def == nullptr) return handleKeysMode(n, state);

    if (n == interfaceButtonSdlList)
    {
        if (ignoreListKey)
        {
            if (state == buttonRelease) ignoreListKey = false;
        }
        else if (!visible)
        {
            if (state == buttonPress) open();
        }
        else if (state == buttonPress)
        {
            if (sticky)
            {
                close();
                ignoreListKey = true;
            }
        }
        else if (state == buttonHold)
        {
            if (!sticky) momentary = true;
        }
        else if (state == buttonRelease && !sticky)
        {
            if (momentary)
            {
                bool run = isEnabled(pageIdx, hl) && current().kind == SdlActionEntry::Action;
                void (*fn)() = current().run;
                close(); // before running, so an action that switches module finds the bar free
                if (run && fn) fn();
            }
            else
            {
                sticky = true;
                refresh();
            }
        }
        return true;
    }

    if (!visible) return false;

    if (n == interfaceButtonPlay || n == interfaceButtonRec) return false;
    if (n == interfaceButtonMiniMenu)
    {
        close();
        return false;
    }

    if (state == buttonRelease) return false; // pressed before the list opened: the module gets its release
    if (state == buttonPress) owned |= bit;

    bool pressOrHold = (state == buttonPress || state == buttonHold);

    switch (n)
    {
    case interfaceButtonLeft:
    case interfaceButtonRight:
        if (pressOrHold)
        {
            seekEnabled(n == interfaceButtonRight ? 1 : -1, true);
            refresh();
        }
        break;
    case interfaceButtonSdlMod:
        if (state == buttonPress)
        {
            pageIdx = (pageIdx + 1) % def->pageCount;
            hl = 0;
            seekEnabled(1, false);
            refresh();
        }
        break;
    case interfaceButtonMiniMiddle:
        if (state == buttonPress && sticky) runHighlighted();
        break;
    case interfaceButtonMiniPlus10:  if (pressOrHold) nudge(10);  break;
    case interfaceButtonMiniPlus1:   if (pressOrHold) nudge(1);   break;
    case interfaceButtonMiniMinus1:  if (pressOrHold) nudge(-1);  break;
    case interfaceButtonMiniMinus10: if (pressOrHold) nudge(-10); break;
    case interfaceButtonUp:          if (pressOrHold) nudge(1);   break;
    case interfaceButtonDown:        if (pressOrHold) nudge(-1);  break;
    default: break;
    }
    return true;
}

bool SdlActionList::handleKeysMode(uint8_t n, uint8_t state)
{
    if (n == interfaceButtonColumn && ignoreColumnKey)
    {
        if (state == buttonRelease) ignoreColumnKey = false;
        return true;
    }
    if (n == interfaceButtonSdlList)
    {
        if (ignoreListKey)
        {
            if (state == buttonRelease) ignoreListKey = false;
        }
        else if (!visible)
        {
            if (state == buttonPress)
            {
                visible = keysMode = true;
                sticky = momentary = false;
                refreshKeysFrame();
            }
        }
        else if (state == buttonPress)
        {
            if (sticky)
            {
                close();
                ignoreListKey = true;
            }
        }
        else if (state == buttonHold)
        {
            if (!sticky) momentary = true;
        }
        else if (state == buttonRelease && !sticky)
        {
            if (momentary)
            {
                const uint8_t key = interfaceButton0 + keysHl;
                close(); // before the tap, so a key that switches module finds the bar free
                mtInterface.ip->processButtonsInput(key, buttonPress);
                mtInterface.ip->processButtonsInput(key, buttonRelease);
            }
            else
            {
                sticky = true;
                refreshKeysFrame();
            }
        }
        return true;
    }

    if (!visible || !keysMode) return false;

    if (n == interfaceButtonColumn && !sticky && state == buttonPress)
    {
        // East (A) while R4 is held cancels: closes without the release tap, R4's release and A's own hold/release are swallowed
        close();
        ignoreListKey = ignoreColumnKey = true;
        return true;
    }
    if (n == interfaceButtonMiniMenu)
    {
        close();
        return false;
    }
    if (n == interfaceButtonLeft || n == interfaceButtonRight)
    {
        if (state == buttonPress || state == buttonHold)
        {
            keysHl = (uint8_t)((keysHl + (n == interfaceButtonRight ? 1 : kCells - 1)) % kCells);
            refreshKeysFrame();
        }
        return true; // their releases too: the page never saw the press
    }
    if (n == interfaceButtonMiniMiddle)
    {
        if (state == buttonPress)
        {
            forwardKey = (int8_t)(interfaceButton0 + keysHl);
            mtInterface.ip->processButtonsInput((uint8_t)forwardKey, buttonPress);
        }
        return true;
    }
    return false; // value keys, Up/Down, Shift, Delete and the rest work the page as usual
}

// One list cell as a frame over the firmware's label under it: border only, no text, no background.
void SdlActionList::refreshKeysFrame()
{
    if (labels[0] == -1) return;
    const uint32_t* mode = sticky ? kBlue : kAmber;
    for (uint8_t i = 0; i < kCells; i++)
    {
        if (i != keysHl)
        {
            display.setControlHide(labels[i]);
            display.refreshControl(labels[i]);
            continue;
        }
        memcpy(cellColors[i], mode, sizeof(cellColors[i]));
        display.setControlText(labels[i], "");
        display.setControlText2(labels[i], "");
        display.setControlValue(labels[i], 0);
        display.setControlColors(labels[i], cellColors[i]);
        display.setControlStyle(labels[i], controlStyleShow | controlStyleCenterX | controlStyleCenterY |
                                               controlStyleFont3 | controlStyleBorder);
        display.refreshControl(labels[i]);
    }
    display.synchronizeRefresh();
}

void SdlActionList::open()
{
    Memory* m = memoryFor(def);
    pageIdx = m->page < def->pageCount ? m->page : 0;
    hl = m->hl < def->pages[pageIdx].count ? m->hl : 0;
    seekEnabled(1, false);
    visible = true;
    sticky = false;
    momentary = false;
    refresh();
}

void SdlActionList::close()
{
    if (!visible) return;

    if (!keysMode)
    {
        Memory* m = memoryFor(def);
        m->page = pageIdx;
        m->hl = hl;
    }
    visible = sticky = momentary = keysMode = false;
    if (def)
        refresh();   // the page's cells stay, tinted dark
    else
        hideCells();
}

void SdlActionList::hideCells()
{
    if (labels[0] == -1) return;
    for (uint8_t i = 0; i < kCells; i++)
    {
        display.setControlHide(labels[i]);
        display.refreshControl(labels[i]);
    }
    display.synchronizeRefresh();
}

void SdlActionList::idleTick(uint32_t nowMs)
{
    // Closed list: repaint the dark cells only when a value or an enabled state changed (checked every 100 ms).
    if (visible || def == nullptr || labels[0] == -1 || nowMs - lastIdleMs < 100) return;
    lastIdleMs = nowMs;
    char sig[sizeof(idleSig)];
    size_t len = (size_t)snprintf(sig, sizeof(sig), "%s|%u|%d|", def->id, pageIdx,
                                  (int)mtPopups.isNoteKeyboardPopupActive());
    const SdlActionPage& pg = def->pages[pageIdx];
    for (uint8_t i = 0; i < pg.count && len < sizeof(sig) - 40; i++)
    {
        sig[len++] = isEnabled(pageIdx, i) ? '1' : '0';
        char t[32] = "";
        if (pg.entries[i].text2) pg.entries[i].text2(t, sizeof(t));
        len += (size_t)snprintf(sig + len, sizeof(sig) - len, "%s|", t);
    }
    if (strcmp(sig, idleSig) == 0) return;
    snprintf(idleSig, sizeof(idleSig), "%s", sig);
    refresh();
}

bool SdlActionList::isEnabled(uint8_t p, uint8_t h) const
{
    const SdlActionEntry& e = def->pages[p].entries[h];
    return e.enabled == nullptr || e.enabled();
}

// Moves the highlight to the next enabled entry in flattened list order, wrapping across
// pages. stepFirst = false accepts the current entry. All disabled: the highlight stays.
void SdlActionList::seekEnabled(int8_t dir, bool stepFirst)
{
    uint16_t total = 0;
    for (uint8_t p = 0; p < def->pageCount; p++) total += def->pages[p].count;

    uint8_t p = pageIdx, h = hl;
    for (uint16_t i = 0; i <= total; i++)
    {
        if (i > 0 || stepFirst)
        {
            if (dir > 0)
            {
                if (++h >= def->pages[p].count)
                {
                    h = 0;
                    p = (p + 1) % def->pageCount;
                }
            }
            else if (h-- == 0)
            {
                p = (p + def->pageCount - 1) % def->pageCount;
                h = def->pages[p].count - 1;
            }
        }
        if (isEnabled(p, h))
        {
            pageIdx = p;
            hl = h;
            return;
        }
    }
}

void SdlActionList::runHighlighted()
{
    if (current().kind != SdlActionEntry::Action || current().run == nullptr || !isEnabled(pageIdx, hl)) return;
    const SdlActionListDef* before = def;
    current().run();
    if (visible && def == before) refresh();
}

void SdlActionList::nudge(int16_t delta)
{
    if (current().kind != SdlActionEntry::Value || current().nudge == nullptr || !isEnabled(pageIdx, hl)) return;
    current().nudge(delta);
    refresh();
}

SdlActionList::Memory* SdlActionList::memoryFor(const SdlActionListDef* d)
{
    for (Memory& m : memory)
        if (m.def == d) return &m;
    for (Memory& m : memory)
        if (m.def == nullptr) return &(m = { d, 0, 0 });
    // shortcut: more lists than slots forget slot 0's position, raise kMemorySlots if lists outgrow it
    return &(memory[0] = { d, 0, 0 });
}

// Short forms for firmware names that do not fit a cell even in the 12 px font (measured with
// getTextWidth in Roboto Mono, the default, which is the wider of the two firmware font families).
static const char* const kShortNames[][2] = {
    { "Project Settings", "Project" },        { "Fn buttons binding", "Fn Buttons" },
    { "Performance presets", "Perf Presets" }, { "Pattern divider", "Pat Divider" },
    { "Anti-aliasing", "Anti-alias" },         { "Extra headroom", "Headroom" },
    { "Display brightness", "Display Brt" },   { "Pads brightness", "Pads Brt" },
    { "Recording options", "Rec Options" },    { "Recording Threshold", "Rec Thresh" },
    { "Numbering mode", "Numbering" },         { "Pattern arrangement", "Pat Arrange" },
    { "Pattern top info", "Pat Top Info" },    { "Line in channel", "Line In Ch" },
    { "Time signature num", "Time Sig Num" },  { "Time signature denum", "Time Sig Den" },
    { "Notes in channel", "Notes In Ch" },     { "Clock sync correction", "Clock Sync" },
    { "Firmware update", "FW Update" },        { "Current version", "Version" },
    { "Start file transfer", "File Transf" },  { "Timestretch Beat", "TStr Beat" },
    { "Timestretch Note", "TStr Note" },
};

uint32_t SdlActionList::fitLine2(char* t, size_t n, uint8_t page, uint8_t pageCount)
{
    constexpr uint16_t kWidth = 88; // the 94 px label minus a margin
    for (const auto& s : kShortNames)
        if (strcmp(t, s[0]) == 0)
        {
            snprintf(t, n, "%s", s[1]);
            break;
        }
    char suffix[8] = "";
    if (page) snprintf(suffix, sizeof(suffix), "%s%u/%u", t[0] ? " " : "", page, pageCount);
    const uint16_t sw14 = getTextWidth(&fonts[2], suffix, strlen(suffix));
    const uint16_t sw12 = getTextWidth(&fonts[4], suffix, strlen(suffix));
    uint32_t font = controlStyleFont3;
    if (getTextWidth(&fonts[2], t, strlen(t)) + sw14 > kWidth)
    {
        font = controlStyleFont5;
        if (getTextWidth(&fonts[4], t, strlen(t)) + sw12 > kWidth)
            t[getMaxTextLengthThatFitSpecificWidth(&fonts[4], t, kWidth > sw12 ? kWidth - sw12 : 0)] = 0;
    }
    size_t len = strlen(t);
    snprintf(t + len, n - len, "%s", suffix);
    return font;
}

void SdlActionList::refresh()
{
    if (labels[0] == -1 || def == nullptr || keysMode) return;

    const bool idle = !visible; // closed: every cell tinted dark, no highlight, values live
    if (idle && mtPopups.isNoteKeyboardPopupActive())
    {
        hideCells();   // the popup's own key labels own the strip meanwhile
        return;
    }
    const SdlActionPage& pg = def->pages[pageIdx];
    const uint32_t* mode = idle ? kIdle : sticky ? kBlue : kAmber;
    const uint32_t* theme = display.getTheme().activeLabelsColors;
    const uint32_t baseStyle = controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3 |
                               controlStyleBorder;

    for (uint8_t i = 0; i < kCells; i++)
    {
        bool exists = i < pg.count;
        bool enabled = exists && isEnabled(pageIdx, i);
        bool lit = !idle && exists && enabled && i == hl;
        const uint32_t* base = idle ? ((exists && !enabled) ? kIdleDim : kIdle) : lit ? mode : (exists && !enabled) ? kDim : theme;

        memcpy(cellColors[i], base, sizeof(cellColors[i]));
        cellColors[i][2] = mode[0];                                  // the frame is the mode colour
        if (cellColors[i][3] == 0) cellColors[i][3] = cellColors[i][0]; // value line: 0 would draw black

        cellText2[i][0] = 0;
        if (exists && pg.entries[i].text2) pg.entries[i].text2(cellText2[i], sizeof(cellText2[i]));
        // Fit line 2 to the cell: the firmware's name abbreviated if it has a short form, then the label's own
        // 14 px font, else the 12 px font, else cut at the measured width; the page indicator always stays.
        uint32_t font2 = fitLine2(cellText2[i], sizeof(cellText2[i]),
                                  (i == kCells - 1 && def->pageCount > 1) ? pageIdx + 1 : 0, def->pageCount);

        // an enabled cell with no line 2 (most actions) draws its label in the value colour, else it reads as disabled
        if (!idle && exists && enabled && !lit && cellText2[i][0] == 0) cellColors[i][0] = cellColors[i][3];

        display.setControlText(labels[i], exists ? pg.entries[i].label : "");
        display.setControlText2(labels[i], cellText2[i]);
        display.setControlStyle2(labels[i], controlStyleCenterX | controlStyleCenterY | font2);
        display.setControlValue(labels[i], cellText2[i][0] ? 1 : 0);
        display.setControlColors(labels[i], cellColors[i]);
        display.setControlStyle(labels[i], baseStyle | (lit ? controlStyleBackground : 0));
        display.refreshControl(labels[i]);
    }
    display.synchronizeRefresh();
}

#endif // PLATFORM_SDL
