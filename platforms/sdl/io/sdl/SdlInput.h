#ifndef PLATFORMS_SDL_IO_SDL_SDLINPUT_H_
#define PLATFORMS_SDL_IO_SDL_SDLINPUT_H_

#include "ButtonsImpl.h"
#include "InputMap.h"

#include <SDL_events.h>
#include <SDL_scancode.h>
#include <cstddef>
#include <cstdint>
#include <vector>

class PlatformImpl;

// Desktop keyboard, the Brick pad (INPUT-MAP.md) and scripted keys (--keys)
// feeding ButtonsImpl. Escape held kPowerHoldMs, or select + start both held that long,
// is the power button; P requests a screenshot. Select and start presses reach the firmware
// kChordDelayMs late (never, when the other one joins within that time), so the chord does
// not also start Rec and Play; a tap released earlier delivers press and release at once.
// Pad events arrive only when main passes SDL_INIT_JOYSTICK to SDL_Init; the first joystick
// SDL reports (joystick 0 at startup) is opened on SDL_JOYDEVICEADDED.
class SdlInput
{
  public:
    static constexpr uint32_t kPowerHoldMs = 500;
    static constexpr uint32_t kChordDelayMs = 150;

    SdlInput(ButtonsImpl& buttons, PlatformImpl& platform);

    // Drains SDL events and fires due script events. nowMs is the caller's clock.
    // runScript false holds the --keys script (main's --from-ready gate); its clock starts at the first true call.
    void pump(uint32_t nowMs, bool runScript = true);
    bool quitRequested() const { return quit; }
    bool takeScaleToggle()   // L4 + Menu since the last call
    {
        bool r = scaleToggle;
        scaleToggle = false;
        return r;
    }
    bool takeScreenshotRequest()
    {
        bool r = screenshot;
        screenshot = false;
        return r;
    }
    // Physical index map for the pad: "brick" (default) or "brickpro". False and no change
    // when the name is unknown.
    bool setDevice(const char* name);
    // One physical pad input (InputMap::kBrickBindings ids) went down or up. Repeats of
    // the current state are ignored. Every input latches its Mini key at press time and
    // releases that same key; f1 (L4) is the mod key and selects the mod column while held.
    // The paste target is Shift + Copy (Shift kept on release while west is down).
    void onPhysical(const char* input, bool down, uint32_t nowMs);
    // For the legend strip: whether L4 (mod) is held, and the active device map.
    bool modHeld() const { return modDown; }
    bool isDown(const char* input) const;   // physical state of one pad input
    const BrickDevice& currentDevice() const { return *device; }
    // Jog wheel steps (Pro right stick) go here as (pot 0, +1 clockwise / -1 anticlockwise);
    // main passes the firmware's onPotChange. nullptr drops them.
    void setPotCallback(void (*cb)(uint8_t, int16_t)) { potCb = cb; }

    // "t:id[:holdMs],..." with t in ms from the first pump(), id a Mini key id, or a pad
    // input as "pad.<input>" (sent through onPhysical, so the mod column applies), or ids joined
    // with '+' (pressed in order, released together in the same order), holdMs default 60.
    // Returns false with a message on stderr when the spec does not parse.
    // shortcut: call before the first pump(), a later add re-sorts fired events; split
    // the fired prefix off if a live script feed is ever wanted.
    bool addScript(const char* spec);

  private:
    struct ScriptEvent
    {
        uint32_t atMs;
        uint8_t index;     // Mini key, when pad is null
        const char* pad;   // physical input id (static string from kBrickBindings) or null
        bool press;
    };

    void key(SDL_Scancode sc, bool down, uint32_t nowMs);
    void joyEvent(const SDL_Event& e, uint32_t nowMs);
    void deliver(size_t row, bool down); // press or release the Mini key of a pad row
    bool stickAxis(uint8_t axis) const;  // the device has an AxisNeg source on this axis
    void wheelMoved();                  // after wheelX or wheelY changed
    void updatePower(uint32_t nowMs); // after escDown or chordDown changed
    bool parseEntry(const char* entry, size_t len);

    ButtonsImpl& buttons;
    PlatformImpl& platform;
    bool quit = false;
    bool screenshot = false;
    bool scaleToggle = false;
    int8_t latched[SDL_NUM_SCANCODES]; // Mini index while the key is held, -1 otherwise
    bool escDown = false;
    bool chordDown = false; // select + start
    bool powerHeld = false; // escDown || chordDown, as last seen by updatePower
    bool powerSent = false;
    uint32_t powerDownMs = 0;

    const BrickDevice* device;
    SDL_Joystick* joy = nullptr;
    int32_t joyId = -1;           // SDL instance id of joy
    bool modDown = false;
    // Per kBrickBindings row (transient runtime index, rebuilt from the table each run).
    std::vector<bool> physDown;
    std::vector<int8_t> physLatched; // Mini index latched at press, -1 otherwise
    std::vector<bool> physPasteShift; // true while the row's paste chord holds a synthesized Shift
    std::vector<uint32_t> physDueMs; // select/start press held back until this time, 0 = none
    size_t selRow, staRow;           // kBrickBindings rows of the power chord keys
    size_t westRow;                  // the Shift row, whose Shift a paste release leaves down
    void (*potCb)(uint8_t, int16_t) = nullptr;
    int wheelX = 0, wheelY = 0;      // last right stick axes
    int wheelStep = -1;              // detent of the last angle, -1 inside kWheelRadius
    std::vector<ScriptEvent> script;
    size_t scriptNext = 0;
    bool scriptStarted = false;
    uint32_t scriptT0 = 0;
};

#endif
