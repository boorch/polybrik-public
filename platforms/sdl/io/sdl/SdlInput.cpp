#include "io/sdl/SdlInput.h"

#include "InputMap.h"
#include "PlatformImpl.h"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

SdlInput::SdlInput(ButtonsImpl& buttons, PlatformImpl& platform)
  : buttons(buttons), platform(platform), device(InputMap::brickDevice("brick")),
    physDown(InputMap::kBrickBindingCount, false), physLatched(InputMap::kBrickBindingCount, -1),
    physPasteShift(InputMap::kBrickBindingCount, false), physDueMs(InputMap::kBrickBindingCount, 0),
    selRow(InputMap::brickBinding("select") - InputMap::kBrickBindings),
    staRow(InputMap::brickBinding("start") - InputMap::kBrickBindings),
    westRow(InputMap::brickBinding("west") - InputMap::kBrickBindings)
{
    memset(latched, -1, sizeof(latched));
}

void SdlInput::deliver(size_t row, bool down)
{
    physDueMs[row] = 0;
    if (down)
    {
        const char* target = InputMap::kBrickBindings[row].target(modDown);
        if (!strcmp(target, InputMap::kScaleId))
        {
            scaleToggle = true;   // platform action, nothing reaches the firmware
            physLatched[row] = -1;
            physPasteShift[row] = false;
            return;
        }
        bool paste = !strcmp(target, InputMap::kPasteId);
        int idx = InputMap::miniIndex(paste ? "copy" : target); // -1 for ""
        physLatched[row] = (int8_t)idx;
        physPasteShift[row] = paste;
        if (paste)
            buttons.press((uint8_t)InputMap::miniIndex("shift")); // no-op when Shift is already down
        if (idx >= 0)
            buttons.press((uint8_t)idx);
    }
    else if (physLatched[row] >= 0)
    {
        buttons.release((uint8_t)physLatched[row]);
        physLatched[row] = -1;
        if (physPasteShift[row] && !physDown[westRow])
            buttons.release((uint8_t)InputMap::miniIndex("shift"));
        physPasteShift[row] = false;
    }
}

bool SdlInput::setDevice(const char* name)
{
    const BrickDevice* d = InputMap::brickDevice(name);
    if (d)
        device = d;
    return d != nullptr;
}

void SdlInput::updatePower(uint32_t nowMs)
{
    bool held = escDown || chordDown;
    if (held && !powerHeld)
        powerDownMs = nowMs;
    // PowerControl.cpp: callback(1) when the button goes down, callback(0) when it comes up.
    if (!held && powerSent)
    {
        powerSent = false;
        fprintf(stderr, "power: released at %u ms, callback(0)\n", nowMs);
        if (platform.getPowerButtonCallback())
            platform.getPowerButtonCallback()(0);
    }
    powerHeld = held;
}

bool SdlInput::isDown(const char* input) const
{
    const BrickBinding* b = InputMap::brickBinding(input);
    return b && physDown[(size_t)(b - InputMap::kBrickBindings)];
}

void SdlInput::onPhysical(const char* input, bool down, uint32_t nowMs)
{
    const BrickBinding* b = InputMap::brickBinding(input);
    if (!b)
        return;
    size_t row = (size_t)(b - InputMap::kBrickBindings);
    if (physDown[row] == down)
        return;
    physDown[row] = down;

    if (!strcmp(b->input, InputMap::kModInput))
    {
        modDown = down; // latched keys keep the target they had at press
        deliver(row, down);
    }
    else if (row == selRow || row == staRow)
    {
        // Power chord: hold the press back kChordDelayMs so the
        // chord never reaches the firmware as Rec and Play. A key the firmware already saw
        // (held longer than the delay before the other joined) is released normally.
        size_t other = row == selRow ? staRow : selRow;
        if (down && physDown[other])
            physDueMs[other] = 0; // chord: the other press is dropped, this one never queued
        else if (down)
            physDueMs[row] = nowMs + kChordDelayMs;
        else if (physDueMs[row])
            deliver(row, true), deliver(row, false); // tap shorter than the delay
        else
            deliver(row, false);
        chordDown = physDown[selRow] && physDown[staRow];
        updatePower(nowMs);
    }
    else
        deliver(row, down);
}

void SdlInput::joyEvent(const SDL_Event& e, uint32_t nowMs)
{
    if (e.type == SDL_JOYDEVICEADDED)
    {
        if (!joy && (joy = SDL_JoystickOpen(e.jdevice.which)))
            joyId = SDL_JoystickInstanceID(joy);
        return;
    }
    // jdevice.which, jbutton.which, jhat.which and jaxis.which are all the instance id.
    if (!joy || e.jdevice.which != joyId)
        return;
    if (e.type == SDL_JOYDEVICEREMOVED)
    {
        for (size_t i = 0; i < InputMap::kBrickBindingCount; i++)
            onPhysical(InputMap::kBrickBindings[i].input, false, nowMs); // no stuck keys
        SDL_JoystickClose(joy);
        joy = nullptr;
        joyId = -1;
        return;
    }
    // shortcut: raw Brick indices, so a desktop pad with another layout maps oddly; add a
    // per-GUID BrickDevice if desktop pads ever matter.
    for (size_t i = 0; i < device->count; i++)
    {
        const JoySource& s = device->sources[i];
        if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP)
        {
            if (s.kind == JoyKind::Button && s.index == e.jbutton.button)
                onPhysical(s.input, e.type == SDL_JOYBUTTONDOWN, nowMs);
        }
        else if (e.type == SDL_JOYHATMOTION)
        {
            if (s.kind == JoyKind::Hat && s.index == e.jhat.hat)
                onPhysical(s.input, (e.jhat.value & s.mask) != 0, nowMs);
        }
        else if (e.type == SDL_JOYAXISMOTION && s.index == e.jaxis.axis)
        {
            const int v = e.jaxis.value;
            if (s.kind == JoyKind::Wheel)
            {
                (!strcmp(s.input, "wheel_x") ? wheelX : wheelY) = v;
                wheelMoved();
            }
            else if (s.kind == JoyKind::Axis && !stickAxis(s.index))
                onPhysical(s.input, v > InputMap::kTriggerThreshold, nowMs);
            else if (s.kind == JoyKind::Axis || s.kind == JoyKind::AxisNeg)
            {
                // Stick direction with hysteresis: one clean press / release per push.
                const int m = s.kind == JoyKind::Axis ? v : -v;
                onPhysical(s.input, isDown(s.input) ? m >= InputMap::kStickOff : m > InputMap::kStickOn, nowMs);
            }
        }
    }
}

bool SdlInput::stickAxis(uint8_t axis) const
{
    for (size_t i = 0; i < device->count; i++)
        if (device->sources[i].kind == JoyKind::AxisNeg && device->sources[i].index == axis)
            return true;
    return false;
}

void SdlInput::wheelMoved()
{
    const int n = InputMap::kWheelDetents;
    const double r = InputMap::kWheelRadius;
    if ((double)wheelX * wheelX + (double)wheelY * wheelY < r * r)
    {
        wheelStep = -1; // dead zone: re-entering the ring starts a fresh count
        return;
    }
    // SDL's y axis points down, so a growing atan2 angle is clockwise on the stick.
    const int step = (int)(((long)lround(atan2(wheelY, wheelX) * n / (2 * M_PI)) % n + n) % n);
    if (wheelStep >= 0 && step != wheelStep)
    {
        int d = ((step - wheelStep) % n + n) % n; // the shorter way round, so the +-pi seam is one step
        if (d > n / 2)
            d -= n;
        for (int i = 0; i < (d < 0 ? -d : d); i++)
            if (potCb)
                potCb(0, d > 0 ? 1 : -1);
    }
    wheelStep = step;
}

void SdlInput::pump(uint32_t nowMs, bool runScript)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
            case SDL_QUIT: quit = true; break;
            case SDL_KEYDOWN:
                if (!e.key.repeat)
                    key(e.key.keysym.scancode, true, nowMs);
                break;
            case SDL_KEYUP: key(e.key.keysym.scancode, false, nowMs); break;
            case SDL_JOYDEVICEADDED:
            case SDL_JOYDEVICEREMOVED:
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            case SDL_JOYHATMOTION:
            case SDL_JOYAXISMOTION: joyEvent(e, nowMs); break;
            default: break;
        }
    }

    for (size_t row : {selRow, staRow})
        if (physDueMs[row] && nowMs >= physDueMs[row])
            deliver(row, true);

    if (powerHeld && !powerSent && nowMs - powerDownMs >= kPowerHoldMs)
    {
        powerSent = true;
        fprintf(stderr, "power: held %u ms at %u ms, callback(1)\n", kPowerHoldMs, nowMs);
        if (platform.getPowerButtonCallback())
            platform.getPowerButtonCallback()(1);
    }

    if (runScript && scriptNext < script.size())
    {
        if (!scriptStarted)
        {
            scriptStarted = true;
            scriptT0 = nowMs;
        }
        while (scriptNext < script.size() && nowMs - scriptT0 >= script[scriptNext].atMs)
        {
            const ScriptEvent& ev = script[scriptNext++];
            if (ev.pad)
                onPhysical(ev.pad, ev.press, nowMs);
            else if (ev.press)
                buttons.press(ev.index);
            else
                buttons.release(ev.index);
        }
    }
}

void SdlInput::key(SDL_Scancode sc, bool down, uint32_t nowMs)
{
    if (sc == SDL_SCANCODE_ESCAPE)
    {
        escDown = down;
        updatePower(nowMs);
        return;
    }
    if (sc == SDL_SCANCODE_P)
    {
        if (down)
            screenshot = true;
        return;
    }
    if (sc < 0 || sc >= SDL_NUM_SCANCODES)
        return;
    if (down)
    {
        int idx = InputMap::desktopIndex(sc);
        if (idx < 0)
            return;
        latched[sc] = (int8_t)idx;
        buttons.press((uint8_t)idx);
    }
    else if (latched[sc] >= 0)
    {
        buttons.release((uint8_t)latched[sc]);
        latched[sc] = -1;
    }
}

static bool parseMs(const char* s, size_t len, uint32_t& out)
{
    if (len == 0 || len > 9)
        return false;
    out = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (s[i] < '0' || s[i] > '9')
            return false;
        out = out * 10 + (uint32_t)(s[i] - '0');
    }
    return true;
}

bool SdlInput::parseEntry(const char* entry, size_t len)
{
    const char* end = entry + len;
    const char* c1 = (const char*)memchr(entry, ':', len);
    if (!c1)
        return false;
    const char* c2 = (const char*)memchr(c1 + 1, ':', (size_t)(end - c1 - 1));
    const char* idsEnd = c2 ? c2 : end;

    uint32_t t, hold = 60;
    if (!parseMs(entry, (size_t)(c1 - entry), t))
        return false;
    if (c2 && (!parseMs(c2 + 1, (size_t)(end - c2 - 1), hold) || hold == 0))
        return false;

    size_t first = script.size();
    for (const char* p = c1 + 1;;)
    {
        const char* plus = (const char*)memchr(p, '+', (size_t)(idsEnd - p));
        const char* idEnd = plus ? plus : idsEnd;
        size_t len = (size_t)(idEnd - p);
        const char* pad = nullptr;
        int idx = -1;
        if (len > 4 && !memcmp(p, "pad.", 4))
        {
            const BrickBinding* b = InputMap::brickBinding(p + 4, len - 4);
            pad = b ? b->input : nullptr;
        }
        else
            idx = InputMap::miniIndex(p, len); // -1 for an empty id too
        if (idx < 0 && !pad)
        {
            script.resize(first);
            return false;
        }
        script.push_back({ t, (uint8_t)(idx < 0 ? 0 : idx), pad, true });
        if (!plus)
            break;
        p = plus + 1;
    }
    size_t pressed = script.size() - first;
    for (size_t i = 0; i < pressed; i++)
        script.push_back({ t + hold, script[first + i].index, script[first + i].pad, false });
    return true;
}

bool SdlInput::addScript(const char* spec)
{
    size_t before = script.size();
    for (const char* p = spec;;)
    {
        const char* comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (!parseEntry(p, len))
        {
            fprintf(stderr, "--keys: bad entry '%.*s' (want t:id[+id][:holdMs], id a Mini key or pad.<input>)\n", (int)len, p);
            script.resize(before);
            return false;
        }
        if (!comma)
            break;
        p = comma + 1;
    }
    // Stable: a release and a press due at the same ms keep their spec order.
    std::stable_sort(script.begin(), script.end(),
                     [](const ScriptEvent& a, const ScriptEvent& b) { return a.atMs < b.atMs; });
    return true;
}
