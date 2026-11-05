#ifndef PLATFORMS_SDL_INPUTMAP_H_
#define PLATFORMS_SDL_INPUTMAP_H_

#include <SDL_scancode.h>
#include <cstddef>
#include <cstdint>

// INPUT-MAP.md: the key map is data. Mini keys are referenced by
// stable string id; the index is the firmware button number from interfaceDefs.h.
struct MiniKey
{
    const char* id;
    uint8_t index;
};

// Desktop keyboard.
struct DesktopKey
{
    SDL_Scancode key;
    const char* miniId;
};

// Brick pad (INPUT-MAP.md v4): physical input id (dpad_up, south, f1, ...) to the Mini key id
// on the base layer and while L4 (f1, the mod key) is held. "" = unbound. kPasteId in the mod
// column is a pseudo id SdlInput resolves itself (it is not in kMiniKeys).
struct BrickBinding
{
    const char* input;
    const char* base;
    const char* mod;
    const char* target(bool modHeld) const { return modHeld ? mod : base; }
};

// Where a physical input comes from on the raw SDL joystick.
enum class JoyKind : uint8_t
{
    Button, // index = raw button
    Hat,    // index = hat, mask = SDL_HAT_* bit
    Axis,    // index = axis, pressed above kTriggerThreshold (stick axes: kStickOn / kStickOff)
    AxisNeg, // index = axis, the negative half of a stick axis, pressed below -kStickOn
    Wheel    // index = axis, analog: one coordinate of the jog wheel, no press
};
struct JoySource
{
    const char* input;
    JoyKind kind;
    uint8_t index;
    uint8_t mask;
};
struct BrickDevice
{
    const char* name; // "brick" or "brickpro", as the DEVICE environment variable
    const JoySource* sources;
    size_t count;
};

namespace InputMap
{
constexpr const char* kModInput = "f1";  // L4: the mod key
constexpr const char* kListInput = "f2"; // R4: the action list key
constexpr const char* kPasteId = "paste"; // pseudo id, resolved by SdlInput
constexpr const char* kScaleId = "scale"; // pseudo id: toggles the screen between stretched and 1:1 (SdlInput)
constexpr int kTriggerThreshold = 16000; // L2/R2 axes
// Pro sticks (tunable, device probe pending). A stick direction goes down above kStickOn
// and up below kStickOff. The right stick is a jog wheel outside kWheelRadius, kWheelDetents
// steps per turn.
constexpr int kStickOn = 20000;
constexpr int kStickOff = 12000;
constexpr int kWheelRadius = 16000;
constexpr int kWheelDetents = 12;

extern const BrickBinding kBrickBindings[]; // its input column is the physical id list
extern const size_t kBrickBindingCount;
extern const BrickDevice kBrickDevices[];
extern const size_t kBrickDeviceCount;

// Row for a physical id, nullptr when unknown. The (id, len) form takes a substring.
const BrickBinding* brickBinding(const char* input, size_t len);
const BrickBinding* brickBinding(const char* input);
const BrickDevice* brickDevice(const char* name);
// Source of a physical id on a device, nullptr when the device lacks it (l3/r3 on "brick").
const JoySource* joySource(const BrickDevice& dev, const char* input);

extern const MiniKey kMiniKeys[];
extern const size_t kMiniKeyCount;
extern const DesktopKey kDesktopKeys[];
extern const size_t kDesktopKeyCount;

// Mini index for an id, -1 when unknown. The (id, len) form takes a substring.
int miniIndex(const char* id, size_t len);
int miniIndex(const char* id);
// Mini index for a desktop scancode, -1 when the key is not in the table.
int desktopIndex(SDL_Scancode key);
} // namespace InputMap

#endif
