// Headless state-machine test for the core-side action list (core patch 24).
// No display init: initDisplayControls() is never called, so drawing is skipped.
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstring>

#include "core/interfaceDefs.h"
#include "core/sdlActionList.h"
#include "core/modulesBase.h"
#include "ButtonsImpl.h"

static int counter = 0, runs_a1 = 0, runs_b1 = 0, runs_a3 = 0;
static bool disable_a1 = false;

static void nudgeV(int16_t d) { counter += d; }
static void runA1() { runs_a1++; }
static void runA3() { runs_a3++; }
static void runB1() { runs_b1++; }
static void runNone() {}
static bool a1Enabled() { return !disable_a1; }
static bool never() { return false; }
static int yes_runs = 0, no_runs = 0;
static void yesFn() { yes_runs++; }
static void noFn() { no_runs++; }

static const SdlActionEntry kPage1[] = {
    { "v", "Value", SdlActionEntry::Value, nullptr, nudgeV, nullptr, nullptr },
    { "a1", "A1", SdlActionEntry::Action, runA1, nullptr, nullptr, a1Enabled },
    { "a2", "A2", SdlActionEntry::Action, runNone, nullptr, nullptr, never },
    { "a3", "A3", SdlActionEntry::Action, runA3, nullptr, nullptr, nullptr },
};
static const SdlActionEntry kPage2[] = {
    { "b1", "B1", SdlActionEntry::Action, runB1, nullptr, nullptr, nullptr },
};
static const SdlActionPage kPages[] = { { kPage1, 4 }, { kPage2, 1 } };
static const SdlActionListDef def = { "test", kPages, 2 };

static void textOn(char* o, size_t n) { std::snprintf(o, n, "Unarmed"); }
static const SdlActionEntry kFull[] = {
    { "f1", "F1", SdlActionEntry::Action, runNone, nullptr, nullptr, nullptr },
    { "f2", "F2", SdlActionEntry::Action, runNone, nullptr, nullptr, nullptr },
    { "f3", "F3", SdlActionEntry::Action, runNone, nullptr, nullptr, nullptr },
    { "f4", "F4", SdlActionEntry::Action, runNone, nullptr, nullptr, nullptr },
    { "f5", "F5", SdlActionEntry::Action, runNone, nullptr, nullptr, nullptr },
    { "f6", "F6", SdlActionEntry::Action, runNone, nullptr, nullptr, nullptr },
    { "f7", "F7", SdlActionEntry::Action, runNone, nullptr, nullptr, nullptr },
    { "f8", "F8", SdlActionEntry::Action, runNone, nullptr, textOn, nullptr },
};
static const SdlActionPage kFullPages[] = { { kFull, 8 }, { kPage2, 1 } };
static const SdlActionListDef fullDef = { "full", kFullPages, 2 };

int main()
{
    sdlActionList.setList(&def);
    assert(!sdlActionList.handleButton(interfaceButtonUp, buttonPress));          // nothing open
    assert(sdlActionList.handleButton(interfaceButtonSdlList, buttonPress) && sdlActionList.isVisible());
    assert(sdlActionList.handleButton(interfaceButtonRight, buttonPress) && sdlActionList.highlight() == 1);
    assert(sdlActionList.handleButton(interfaceButtonRight, buttonPress) && sdlActionList.highlight() == 3); // a2 skipped
    assert(sdlActionList.handleButton(interfaceButtonRight, buttonPress) && sdlActionList.page() == 1 && sdlActionList.highlight() == 0);
    assert(sdlActionList.handleButton(interfaceButtonSdlList, buttonHold));
    assert(sdlActionList.handleButton(interfaceButtonSdlList, buttonRelease) && runs_b1 == 1 && !sdlActionList.isVisible());
    // sticky: tap, Enter runs, R4 closes, trailing hold/release ignored
    sdlActionList.handleButton(interfaceButtonSdlList, buttonPress);
    sdlActionList.handleButton(interfaceButtonSdlList, buttonRelease);
    assert(sdlActionList.isVisible() && sdlActionList.isSticky() && sdlActionList.page() == 1);   // remembered
    assert(sdlActionList.handleButton(interfaceButtonMiniMiddle, buttonPress) && runs_b1 == 2 && sdlActionList.isVisible());
    assert(sdlActionList.handleButton(interfaceButtonSdlList, buttonPress) && !sdlActionList.isVisible());
    assert(sdlActionList.handleButton(interfaceButtonSdlList, buttonHold) && !sdlActionList.isVisible());
    assert(sdlActionList.handleButton(interfaceButtonSdlList, buttonRelease) && !sdlActionList.isVisible());
    // value nudges, pass-through keys, Menu closes
    sdlActionList.handleButton(interfaceButtonSdlList, buttonPress); sdlActionList.handleButton(interfaceButtonSdlList, buttonRelease);
    sdlActionList.handleButton(interfaceButtonSdlMod, buttonPress);                                // page flip back to 0
    assert(sdlActionList.page() == 0 && sdlActionList.highlight() == 0);
    assert(sdlActionList.handleButton(interfaceButtonMiniPlus10, buttonPress) && counter == 10);
    assert(sdlActionList.handleButton(interfaceButtonDown, buttonHold) && counter == 9);
    assert(!sdlActionList.handleButton(interfaceButtonPlay, buttonPress));
    assert(sdlActionList.handleButton(interfaceButtonShift, buttonPress));
    assert(!sdlActionList.handleButton(interfaceButtonMiniMenu, buttonPress) && !sdlActionList.isVisible());
    // a key the list took on press keeps its hold and release after the list closed
    assert(sdlActionList.handleButton(interfaceButtonShift, buttonHold));
    assert(sdlActionList.handleButton(interfaceButtonShift, buttonRelease));
    assert(!sdlActionList.handleButton(interfaceButtonShift, buttonPress));   // released: the module owns it again
    assert(!sdlActionList.handleButton(interfaceButtonShift, buttonRelease));
    assert(!sdlActionList.handleButton(interfaceButtonMiniMenu, buttonRelease)); // Menu's press went through, so does its release
    // momentary release in the same tick as the first hold: runs once
    sdlActionList.handleButton(interfaceButtonSdlList, buttonPress);
    sdlActionList.handleButton(interfaceButtonRight, buttonPress);
    sdlActionList.handleButton(interfaceButtonSdlList, buttonHold); sdlActionList.handleButton(interfaceButtonSdlList, buttonRelease);
    assert(runs_a1 == 1);
    // a disabled saved highlight: reopen lands on the next enabled entry
    disable_a1 = true; sdlActionList.handleButton(interfaceButtonSdlList, buttonPress);
    assert(sdlActionList.highlight() == 3);
    sdlActionList.setList(nullptr); assert(!sdlActionList.isVisible());

    // a key pressed before the list opened: its holds stay with the list, its release reaches the module
    disable_a1 = false;
    sdlActionList.setList(&def);
    assert(!sdlActionList.handleButton(interfaceButtonInsert, buttonPress));
    sdlActionList.handleButton(interfaceButtonSdlList, buttonPress); sdlActionList.handleButton(interfaceButtonSdlList, buttonRelease);
    assert(sdlActionList.isSticky());
    assert(sdlActionList.handleButton(interfaceButtonInsert, buttonHold));
    assert(!sdlActionList.handleButton(interfaceButtonInsert, buttonRelease));

    // hold auto-repeat through the real hold curve (ButtonsImpl, the same code on the device):
    // Right and a value key held for one second repeat, not just fire once
    static ButtonsImpl keys;
    keys.setActionCallback(SwitchGroup::kPress, [](uint8_t n) { sdlActionList.handleButton(n, buttonPress); });
    keys.setActionCallback(SwitchGroup::kHold, [](uint8_t n) { sdlActionList.handleButton(n, buttonHold); });
    keys.setActionCallback(SwitchGroup::kRelease, [](uint8_t n) { sdlActionList.handleButton(n, buttonRelease); });
    uint32_t now = 0;
    keys.tick(now);
    keys.press(interfaceButtonRight);
    int moves = 1; // the press itself; every hold event after it changes the highlight too
    uint16_t pos = sdlActionList.page() * 16 + sdlActionList.highlight();
    for (int i = 0; i < 100; i++)
    {
        keys.tick(now += 10);
        uint16_t p = sdlActionList.page() * 16 + sdlActionList.highlight();
        if (p != pos) moves++;
        pos = p;
    }
    keys.release(interfaceButtonRight);
    assert(moves >= 3);
    while (!(sdlActionList.page() == 0 && sdlActionList.highlight() == 0))   // back to the value entry
    {
        sdlActionList.handleButton(interfaceButtonRight, buttonPress);
        sdlActionList.handleButton(interfaceButtonRight, buttonRelease);
    }
    int before = counter;
    keys.press(interfaceButtonMiniPlus1);
    for (int i = 0; i < 100; i++) keys.tick(now += 10);
    keys.release(interfaceButtonMiniPlus1);
    assert(counter - before >= 3);
    assert(sdlActionList.isVisible());
    sdlActionList.setList(nullptr);

    // two-button windows (core patch 27): Enter and Delete answer the window while its 6/7 bindings stand
    static InputProcessor yn(1, interfaceButtonsCount, 1);
    yn.setButtonObj(interfaceButton6, buttonPress, noFn);
    yn.setButtonObj(interfaceButton7, buttonPress, yesFn);
    assert(yn.sdlYesNoTarget(interfaceButtonMiniMiddle) == 0xFF);                   // nothing set
    yn.sdlSetYesNo(interfaceButton7, interfaceButton6);
    assert(yn.sdlYesNoTarget(interfaceButtonMiniMiddle) == interfaceButton7);
    assert(yn.sdlYesNoTarget(interfaceButtonDelete) == interfaceButton6);
    assert(yn.sdlYesNoTarget(interfaceButtonUp) == 0xFF);
    yn.blockAllInputsExcept(interfaceButton6, interfaceButton7);
    yn.processButtonsInput(yn.sdlYesNoTarget(interfaceButtonMiniMiddle), buttonPress);
    yn.processButtonsInput(yn.sdlYesNoTarget(interfaceButtonDelete), buttonPress);
    assert(yes_runs == 1 && no_runs == 1);
    yn.unblockAllInputs();
    yn.clearButtonsRange(interfaceButton0, interfaceButton7);                       // window torn down
    assert(yn.sdlYesNoTarget(interfaceButtonMiniMiddle) == 0xFF && yn.sdlYesNoTarget(interfaceButtonDelete) == 0xFF);
    yn.setButtonObj(interfaceButton6, buttonPress, noFn);
    yn.setButtonObj(interfaceButton7, buttonPress, yesFn);
    yn.sdlSetYesNo(interfaceButton7, interfaceButton6);
    yn.setButtonObj(interfaceButton7, buttonPress, runNone);                        // 7 rebound by the next screen
    assert(yn.sdlYesNoTarget(interfaceButtonMiniMiddle) == 0xFF);

    // page indicator: follows cell 8's own line 2 instead of replacing it (needs the cells, so last)
    sdlActionList.initDisplayControls();
    sdlActionList.setList(&fullDef);
    sdlActionList.handleButton(interfaceButtonSdlList, buttonPress); sdlActionList.handleButton(interfaceButtonSdlList, buttonRelease);
    assert(std::strcmp(sdlActionList.cellLine2(7), "Unarmed 1/2") == 0);
    sdlActionList.handleButton(interfaceButtonSdlMod, buttonPress);
    assert(std::strcmp(sdlActionList.cellLine2(7), "2/2") == 0);
    sdlActionList.setList(nullptr);

    assert(runs_a3 == 0);
    std::printf("actionlist_test: all checks passed\n");
    return 0;
}
