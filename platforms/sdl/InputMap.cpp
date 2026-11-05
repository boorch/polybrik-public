#include "InputMap.h"

#include "core/interfaceDefs.h"

#include <SDL_joystick.h>
#include <cstring>

namespace InputMap
{
const MiniKey kMiniKeys[] = {
    { "up", interfaceButtonUp },
    { "down", interfaceButtonDown },
    { "left", interfaceButtonLeft },
    { "right", interfaceButtonRight },
    { "enter", interfaceButtonMiniMiddle },
    { "shift", interfaceButtonShift },
    { "note", interfaceButtonNote },
    { "instr", interfaceButtonInstr },
    { "fx1", interfaceButtonFx1 },
    { "fx2", interfaceButtonFx2 },
    { "play", interfaceButtonPlay },
    { "rec", interfaceButtonRec },
    { "copy", interfaceButtonCopy },
    { "insert", interfaceButtonInsert },
    { "delete", interfaceButtonDelete },
    { "menu", interfaceButtonMiniMenu },
    { "fn1", interfaceButtonMiniFn1 },
    { "fn2", interfaceButtonMiniFn2 },
    { "fn3", interfaceButtonMiniFn3 },
    { "fn4", interfaceButtonMiniFn4 },
    { "plus1", interfaceButtonMiniPlus1 },
    { "minus1", interfaceButtonMiniMinus1 },
    { "plus10", interfaceButtonMiniPlus10 },
    { "minus10", interfaceButtonMiniMinus10 },
    { "screen1", interfaceButton0 },
    { "screen2", interfaceButton1 },
    { "screen3", interfaceButton2 },
    { "screen4", interfaceButton3 },
    { "screen5", interfaceButton4 },
    { "screen6", interfaceButton5 },
    { "screen7", interfaceButton6 },
    { "screen8", interfaceButton7 },
    { "game", interfaceButtonGame },
    { "column", interfaceButtonColumn }, // SDL only, core patch 20
    { "list", interfaceButtonSdlList }, // SDL only, core patch 23
    { "mod", interfaceButtonSdlMod },
    { "preview", interfaceButtonSdlPreview },
    { "undo", interfaceButtonSdlUndo },
    { "redo", interfaceButtonSdlRedo },
    // Desktop (Tracker) module keys, reached on the Mini through Fn bindings.
    { "pattern", interfaceButtonPattern },
    { "song", interfaceButtonSong },
    { "file", interfaceButtonFile },
    { "config", interfaceButtonConfig },
    { "master", interfaceButtonMaster },
    { "params", interfaceButtonParams },
    { "perf", interfaceButtonPerformance },
    { "sample_load", interfaceButtonSampleLoad },
    { "sample_play", interfaceButtonSamplePlay },
    { "sample_edit", interfaceButtonSampleEdit },
    { "sample_rec", interfaceButtonSampleRec },
};
const size_t kMiniKeyCount = sizeof(kMiniKeys) / sizeof(kMiniKeys[0]);

// Desktop keyboard. Escape (power) and P (screenshot) are handled by SdlInput, not here.
const DesktopKey kDesktopKeys[] = {
    { SDL_SCANCODE_UP, "up" },
    { SDL_SCANCODE_DOWN, "down" },
    { SDL_SCANCODE_LEFT, "left" },
    { SDL_SCANCODE_RIGHT, "right" },
    { SDL_SCANCODE_RETURN, "enter" },
    { SDL_SCANCODE_LSHIFT, "shift" },
    { SDL_SCANCODE_C, "copy" },
    { SDL_SCANCODE_DELETE, "delete" },
    { SDL_SCANCODE_INSERT, "insert" },
    { SDL_SCANCODE_SPACE, "play" },
    { SDL_SCANCODE_R, "rec" },
    { SDL_SCANCODE_M, "menu" },
    { SDL_SCANCODE_N, "note" },
    { SDL_SCANCODE_I, "instr" },
    { SDL_SCANCODE_V, "fx2" },
    { SDL_SCANCODE_X, "fx1" },
    { SDL_SCANCODE_F1, "fn1" },
    { SDL_SCANCODE_F2, "fn2" },
    { SDL_SCANCODE_F3, "fn3" },
    { SDL_SCANCODE_F4, "fn4" },
    { SDL_SCANCODE_F5, "screen1" },
    { SDL_SCANCODE_F6, "screen2" },
    { SDL_SCANCODE_F7, "screen3" },
    { SDL_SCANCODE_F8, "screen4" },
    { SDL_SCANCODE_F9, "screen5" },
    { SDL_SCANCODE_F10, "screen6" },
    { SDL_SCANCODE_F11, "screen7" },
    { SDL_SCANCODE_F12, "screen8" },
    { SDL_SCANCODE_LEFTBRACKET, "minus1" },
    { SDL_SCANCODE_RIGHTBRACKET, "plus1" },
    { SDL_SCANCODE_PAGEDOWN, "minus10" },
    { SDL_SCANCODE_PAGEUP, "plus10" },
    { SDL_SCANCODE_TAB, "list" },
    { SDL_SCANCODE_LALT, "mod" },
    { SDL_SCANCODE_B, "preview" },
    { SDL_SCANCODE_Z, "undo" },
    { SDL_SCANCODE_Y, "redo" },
};
const size_t kDesktopKeyCount = sizeof(kDesktopKeys) / sizeof(kDesktopKeys[0]);

// INPUT-MAP.md table (v4). Columns: input, base, mod (L4 held). "" = unbound
// under that column. The shoulders are the value keys, L4 is the one mod column, R4 opens the
// action list; Shift (west), Play and Rec pass through the mod column.
const BrickBinding kBrickBindings[] = {
    { "dpad_up", "up", "fn1" },       // L4 + D-pad: Fn1..Fn4; Y held first: Shift + Fn = Fn5..Fn8
    { "dpad_down", "down", "fn3" },
    { "dpad_left", "left", "fn4" },
    { "dpad_right", "right", "fn2" },
    { "l1", "minus1", "copy" },
    { "l2", "minus10", InputMap::kPasteId },
    { "r2", "plus10", "insert" },
    { "r1", "plus1", "preview" },
    { "south", "enter", "redo" },    // L4 + B = Redo
    { "east", "column", "" }, // A: the cursor column's key held (core patch 20)
    { "west", "shift", "shift" },
    { "north", "delete", "undo" },   // L4 + X = Undo (X is Delete)
    { "start", "play", "play" },
    { "select", "rec", "rec" },
    { "menu", "menu", InputMap::kScaleId }, // L4 + Menu: screen stretched / 1:1
    { "f1", "mod", "mod" }, // L4: sets the mod column and reaches the firmware as the modifier key
    { "f2", "list", "list" }, // R4: the action list key
    { "l3", "", "" }, // Pro stick clicks, unbound
    { "r3", "preview", "" },
    { "ls_left", "note", "" }, // Pro left stick: the column keys
    { "ls_right", "instr", "" },
    { "ls_up", "fx1", "" },
    { "ls_down", "fx2", "" },
};
const size_t kBrickBindingCount = sizeof(kBrickBindings) / sizeof(kBrickBindings[0]);

// Raw joystick indices: D-pad hat 0, L2/R2 axes 2/5. The two
// devices differ only from b9 up (plain: under-screen keys; Pro: stick clicks, then b11/b12).
static const JoySource kBrickSources[] = {
    { "dpad_up", JoyKind::Hat, 0, SDL_HAT_UP },
    { "dpad_down", JoyKind::Hat, 0, SDL_HAT_DOWN },
    { "dpad_left", JoyKind::Hat, 0, SDL_HAT_LEFT },
    { "dpad_right", JoyKind::Hat, 0, SDL_HAT_RIGHT },
    { "south", JoyKind::Button, 0, 0 },
    { "east", JoyKind::Button, 1, 0 },
    { "west", JoyKind::Button, 2, 0 },
    { "north", JoyKind::Button, 3, 0 },
    { "l1", JoyKind::Button, 4, 0 },
    { "r1", JoyKind::Button, 5, 0 },
    { "select", JoyKind::Button, 6, 0 },
    { "start", JoyKind::Button, 7, 0 },
    { "menu", JoyKind::Button, 8, 0 },
    { "l2", JoyKind::Axis, 2, 0 },
    { "r2", JoyKind::Axis, 5, 0 },
    { "f1", JoyKind::Button, 9, 0 },
    { "f2", JoyKind::Button, 10, 0 },
};
static const JoySource kBrickProSources[] = {
    { "dpad_up", JoyKind::Hat, 0, SDL_HAT_UP },
    { "dpad_down", JoyKind::Hat, 0, SDL_HAT_DOWN },
    { "dpad_left", JoyKind::Hat, 0, SDL_HAT_LEFT },
    { "dpad_right", JoyKind::Hat, 0, SDL_HAT_RIGHT },
    { "south", JoyKind::Button, 0, 0 },
    { "east", JoyKind::Button, 1, 0 },
    { "west", JoyKind::Button, 2, 0 },
    { "north", JoyKind::Button, 3, 0 },
    { "l1", JoyKind::Button, 4, 0 },
    { "r1", JoyKind::Button, 5, 0 },
    { "select", JoyKind::Button, 6, 0 },
    { "start", JoyKind::Button, 7, 0 },
    { "menu", JoyKind::Button, 8, 0 },
    { "l2", JoyKind::Axis, 2, 0 },
    { "r2", JoyKind::Axis, 5, 0 },
    { "l3", JoyKind::Button, 9, 0 },
    { "r3", JoyKind::Button, 10, 0 },
    { "f1", JoyKind::Button, 11, 0 },
    { "f2", JoyKind::Button, 12, 0 },
    { "ls_left", JoyKind::AxisNeg, 0, 0 },
    { "ls_right", JoyKind::Axis, 0, 0 },
    { "ls_up", JoyKind::AxisNeg, 1, 0 },
    { "ls_down", JoyKind::Axis, 1, 0 },
    { "wheel_x", JoyKind::Wheel, 3, 0 },
    { "wheel_y", JoyKind::Wheel, 4, 0 },
};
const BrickDevice kBrickDevices[] = {
    { "brick", kBrickSources, sizeof(kBrickSources) / sizeof(kBrickSources[0]) },
    { "brickpro", kBrickProSources, sizeof(kBrickProSources) / sizeof(kBrickProSources[0]) },
};
const size_t kBrickDeviceCount = sizeof(kBrickDevices) / sizeof(kBrickDevices[0]);

const BrickBinding* brickBinding(const char* input, size_t len)
{
    for (size_t i = 0; i < kBrickBindingCount; i++)
        if (strlen(kBrickBindings[i].input) == len && !memcmp(kBrickBindings[i].input, input, len))
            return &kBrickBindings[i];
    return nullptr;
}

const BrickBinding* brickBinding(const char* input) { return brickBinding(input, strlen(input)); }

const BrickDevice* brickDevice(const char* name)
{
    for (size_t i = 0; i < kBrickDeviceCount; i++)
        if (!strcmp(kBrickDevices[i].name, name))
            return &kBrickDevices[i];
    return nullptr;
}

const JoySource* joySource(const BrickDevice& dev, const char* input)
{
    for (size_t i = 0; i < dev.count; i++)
        if (!strcmp(dev.sources[i].input, input))
            return &dev.sources[i];
    return nullptr;
}

int miniIndex(const char* id, size_t len)
{
    for (size_t i = 0; i < kMiniKeyCount; i++)
        if (strlen(kMiniKeys[i].id) == len && !memcmp(kMiniKeys[i].id, id, len))
            return kMiniKeys[i].index;
    return -1;
}

int miniIndex(const char* id) { return miniIndex(id, strlen(id)); }

int desktopIndex(SDL_Scancode key)
{
    for (size_t i = 0; i < kDesktopKeyCount; i++)
        if (kDesktopKeys[i].key == key)
            return miniIndex(kDesktopKeys[i].miniId);
    return -1;
}
} // namespace InputMap
