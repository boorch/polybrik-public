// Assert-based self-check of InputMap and SdlInput (keyboard events, Escape power
// hold, --keys script, Brick pad table, layer, power chord, virtual joystick) against a
// real ButtonsImpl and PlatformImpl.
#undef NDEBUG
#include "InputMap.h"
#include "NullDisplayDriver.h"
#include "PlatformImpl.h"
#include "io/sdl/SdlInput.h"

#include <SDL.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::string g_log; // "p35 r35 ..." in callback order
static std::vector<int> g_power;
static void onPush(uint8_t n) { g_log += "p" + std::to_string(n) + " "; }
static void onRelease(uint8_t n) { g_log += "r" + std::to_string(n) + " "; }
static void onPower(int v) { g_power.push_back(v); }
static void onHold(uint8_t n) { g_log += "h" + std::to_string(n) + " "; }
static std::vector<int> g_pots; // onPotChange values (pot 0 only)
static void onPot(uint8_t n, int16_t v)
{
    assert(n == 0);
    g_pots.push_back(v);
}

// "p<idx> " / "r<idx> " for a Mini id, as the callbacks log them.
static std::string P(const char* id) { return "p" + std::to_string(InputMap::miniIndex(id)) + " "; }
static std::string R(const char* id) { return "r" + std::to_string(InputMap::miniIndex(id)) + " "; }

static void pushKey(SDL_Scancode sc, bool down, bool repeat = false)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.repeat = repeat;
    e.key.keysym.scancode = sc;
    int r = SDL_PushEvent(&e);
    assert(r == 1);
}

// Mini ids reachable from the Brick table on device dev (base or mod column).
static std::string unreachable(const BrickDevice& dev)
{
    std::string out;
    for (size_t i = 0; i < InputMap::kMiniKeyCount; i++)
    {
        const char* id = InputMap::kMiniKeys[i].id;
        bool hit = false;
        for (size_t j = 0; j < InputMap::kBrickBindingCount; j++)
        {
            const BrickBinding& b = InputMap::kBrickBindings[j];
            if (InputMap::joySource(dev, b.input) &&
                (!strcmp(b.base, id) || !strcmp(b.mod, id)))
                hit = true;
        }
        if (!hit)
            out += std::string(out.empty() ? "" : " ") + id;
    }
    return out;
}

int main()
{
    // --- InputMap: every id resolves to its own valid index, ids are unique
    assert(InputMap::kMiniKeyCount == 50);
    for (size_t i = 0; i < InputMap::kMiniKeyCount; i++)
    {
        const MiniKey& k = InputMap::kMiniKeys[i];
        assert(k.index < ButtonsImpl::kButtonCount);
        assert(InputMap::miniIndex(k.id) == k.index);
        for (size_t j = 0; j < i; j++)
        {
            assert(strcmp(InputMap::kMiniKeys[j].id, k.id) != 0);
            assert(InputMap::kMiniKeys[j].index != k.index);
        }
    }
    assert(InputMap::miniIndex("up") == 26 && InputMap::miniIndex("enter") == 33);
    assert(InputMap::miniIndex("screen1") == 0 && InputMap::miniIndex("screen8") == 7);
    assert(InputMap::miniIndex("minus10") == 42 && InputMap::miniIndex("game") == 43);
    assert(InputMap::miniIndex("column") == 44 && ButtonsImpl::kButtonCount == 50);
    assert(InputMap::miniIndex("list") == 45 && InputMap::miniIndex("redo") == 49);
    assert(InputMap::miniIndex("nope") == -1 && InputMap::miniIndex("") == -1);
    assert(InputMap::miniIndex("upx", 2) == 26);

    // --- keyboard table: every scancode maps to a valid Mini index, no duplicates
    assert(InputMap::kDesktopKeyCount == 37);
    for (size_t i = 0; i < InputMap::kDesktopKeyCount; i++)
    {
        const DesktopKey& k = InputMap::kDesktopKeys[i];
        int idx = InputMap::desktopIndex(k.key);
        assert(idx >= 0 && idx < ButtonsImpl::kButtonCount);
        assert(idx == InputMap::miniIndex(k.miniId));
        assert(SDL_GetScancodeName(k.key)[0] != '\0');
        for (size_t j = 0; j < i; j++)
            assert(InputMap::kDesktopKeys[j].key != k.key);
    }
    assert(InputMap::desktopIndex(SDL_SCANCODE_ESCAPE) == -1);
    assert(InputMap::desktopIndex(SDL_SCANCODE_P) == -1);
    assert(InputMap::desktopIndex(SDL_SCANCODE_F5) == 0);
    assert(InputMap::desktopIndex(SDL_SCANCODE_PAGEUP) == 39);


    // --- Brick table: every physical id once, every target a known Mini id or ""
    const char* physIds[] = { "dpad_up", "dpad_down", "dpad_left", "dpad_right", "south", "east",
                              "west", "north", "l1", "r1", "l2", "r2", "select", "start", "menu",
                              "f1", "f2", "l3", "r3", "ls_left", "ls_right", "ls_up", "ls_down" };
    assert(InputMap::kBrickBindingCount == sizeof(physIds) / sizeof(physIds[0]));
    for (const char* id : physIds)
    {
        const BrickBinding* b = InputMap::brickBinding(id);
        assert(b && !strcmp(b->input, id));
        assert(!b->base[0] || InputMap::miniIndex(b->base) >= 0);
        assert(!b->mod[0] || !strcmp(b->mod, InputMap::kPasteId) || !strcmp(b->mod, InputMap::kScaleId) || InputMap::miniIndex(b->mod) >= 0);
    }
    assert(!InputMap::brickBinding("nope") && InputMap::brickBinding("southx", 5));
    assert(!strcmp(InputMap::brickBinding(InputMap::kModInput)->base, "mod"));
    assert(!strcmp(InputMap::brickBinding(InputMap::kListInput)->base, "list"));
    assert(!strcmp(InputMap::brickBinding("l2")->mod, "paste") && InputMap::miniIndex("paste") == -1);
    assert(!strcmp(InputMap::brickBinding("dpad_up")->mod, "fn1") && !strcmp(InputMap::brickBinding("dpad_right")->mod, "fn2"));
    assert(!strcmp(InputMap::brickBinding("dpad_down")->mod, "fn3") && !strcmp(InputMap::brickBinding("dpad_left")->mod, "fn4"));
    assert(!strcmp(InputMap::brickBinding("north")->mod, "undo") && !strcmp(InputMap::brickBinding("south")->mod, "redo"));
    assert(!strcmp(InputMap::brickBinding("west")->mod, "shift"));
    assert(!strcmp(InputMap::brickBinding("l1")->base, "minus1") && !strcmp(InputMap::brickBinding("r2")->base, "plus10"));
    assert(!strcmp(InputMap::brickBinding("r1")->mod, "preview") && !strcmp(InputMap::brickBinding("r3")->base, "preview"));
    assert(!strcmp(InputMap::brickBinding("select")->mod, "rec"));
    assert(!strcmp(InputMap::brickBinding("south")->base, "enter") && !strcmp(InputMap::brickBinding("east")->base, "column"));
    assert(!InputMap::brickBinding("l3")->base[0] && !InputMap::brickBinding("l3")->mod[0]);
    assert(!strcmp(InputMap::brickBinding("ls_left")->base, "note") && !strcmp(InputMap::brickBinding("ls_right")->base, "instr"));
    assert(!strcmp(InputMap::brickBinding("ls_up")->base, "fx1") && !strcmp(InputMap::brickBinding("ls_down")->base, "fx2"));
    assert(!InputMap::brickBinding("ls_up")->mod[0]);

    // --- device maps: every source is a known physical id, no source used twice; brick has
    // everything but l3/r3, brickpro everything; the indices that differ per device
    const BrickDevice* brick = InputMap::brickDevice("brick");
    const BrickDevice* pro = InputMap::brickDevice("brickpro");
    assert(brick && pro && !InputMap::brickDevice("nope"));
    for (const BrickDevice* d : { brick, pro })
        for (size_t i = 0; i < d->count; i++)
        {
            const JoySource& s = d->sources[i];
            assert(s.kind == JoyKind::Wheel || InputMap::brickBinding(s.input)); // the wheel feeds pots, not a binding row
            assert(InputMap::joySource(*d, s.input) == &s);
            for (size_t j = 0; j < i; j++)
                assert(d->sources[j].kind != s.kind || d->sources[j].index != s.index ||
                       d->sources[j].mask != s.mask);
        }
    for (const char* id : physIds)
    {
        bool stick = !strcmp(id, "l3") || !strcmp(id, "r3") || !strncmp(id, "ls_", 3);
        assert((InputMap::joySource(*brick, id) != nullptr) == !stick);
        assert(InputMap::joySource(*pro, id) != nullptr);
    }
    assert(InputMap::joySource(*brick, "f1")->index == 9 && InputMap::joySource(*brick, "f2")->index == 10);
    assert(InputMap::joySource(*pro, "f1")->index == 11 && InputMap::joySource(*pro, "f2")->index == 12);
    assert(InputMap::joySource(*pro, "l3")->index == 9 && InputMap::joySource(*pro, "r3")->index == 10);
    assert(InputMap::joySource(*pro, "l2")->kind == JoyKind::Axis && InputMap::joySource(*pro, "r2")->index == 5);
    assert(InputMap::joySource(*brick, "dpad_left")->kind == JoyKind::Hat);
    assert(InputMap::joySource(*pro, "ls_left")->kind == JoyKind::AxisNeg && InputMap::joySource(*pro, "ls_down")->index == 1);
    assert(InputMap::joySource(*pro, "wheel_x")->kind == JoyKind::Wheel && InputMap::joySource(*pro, "wheel_y")->index == 4);
    assert(!InputMap::joySource(*brick, "wheel_x"));

    // --- reachability of the 50 Mini keys
    std::string unBrick = unreachable(*brick), unPro = unreachable(*pro);
    printf("unreachable on brick: %s\n", unBrick.c_str());
    printf("unreachable on brickpro: %s\n", unPro.c_str());
    const char* modules = "pattern song file config master params perf sample_load sample_play "
                          "sample_edit sample_rec";
    const char* common = "screen1 screen2 screen3 screen4 screen5 screen6 screen7 screen8 game ";
    assert(unBrick == std::string("note instr fx1 fx2 ") + common + modules); // note / instr / fx keys: A (column); modules: Menu grid
    assert(unPro == std::string(common) + modules); // note / instr / fx keys: left stick on the Pro

    int r = SDL_Init(SDL_INIT_EVENTS);
    assert(r == 0);
    NullDisplayDriver nullDisplay;
    PlatformImpl platform("/nonexistent-sd", &nullDisplay);
    platform.setPowerButtonCallback(onPower);
    ButtonsImpl& buttons = platform.buttonsImpl();
    buttons.setActionCallback(SwitchGroup::kPress, onPush);
    buttons.setActionCallback(SwitchGroup::kRelease, onRelease);

    // --- hold curve: Up and Enter repeat after the hold time
    {
        buttons.setActionCallback(SwitchGroup::kHold, onHold);
        buttons.tick(0);
        buttons.press(interfaceButtonUp);
        buttons.press(interfaceButtonMiniMiddle);
        for (uint32_t t = 10; t <= 2000; t += 10)
            buttons.tick(t);
        buttons.release(interfaceButtonUp);
        buttons.release(interfaceButtonMiniMiddle);
        assert(g_log.rfind("p26 p33 h26 h33 ", 0) == 0);
        assert(g_log.substr(g_log.size() - 8) == "r26 r33 ");
        buttons.setActionCallback(SwitchGroup::kHold, nullptr);
        g_log.clear();
        printf("hold curve: Up and Enter repeat\n");
    }

    // --- script parsing: bad specs leave nothing behind, good ones run
    {
        SdlInput in(buttons, platform);
        const char* bad[] = { "", "500", "500:", "abc:fn1", "500:nope", "500:fn1:xx",
                              "500:fn1:0", "500:fn1+", "500:fn1,", "500:fn1+nope", "500::60" };
        for (const char* s : bad)
        {
            bool ok = in.addScript(s);
            assert(!ok);
        }
        for (uint32_t t = 0; t < 3000; t += 10)
            in.pump(t);
        assert(g_log.empty());
    }

    // --- scripted run: press/release order and isButtonPressed between them
    {
        SdlInput in(buttons, platform);
        bool ok = in.addScript("500:fn1,1200:down:80,1500:shift+note");
        assert(ok);
        in.pump(100); // first pump sets t0 = 100
        assert(g_log.empty());
        in.pump(599);
        assert(g_log.empty() && !buttons.isButtonPressed(35));
        in.pump(600);
        assert(g_log == "p35 " && buttons.isButtonPressed(35));
        in.pump(659);
        assert(buttons.isButtonPressed(35));
        in.pump(660);
        assert(g_log == "p35 r35 " && !buttons.isButtonPressed(35));
        in.pump(1300);
        assert(g_log == "p35 r35 p31 " && buttons.isButtonPressed(31));
        in.pump(1379);
        assert(buttons.isButtonPressed(31));
        in.pump(1380);
        assert(!buttons.isButtonPressed(31));
        in.pump(1600);
        assert(buttons.isButtonPressed(29) && buttons.isButtonPressed(18));
        in.pump(1700); // several due events in one pump
        assert(g_log == "p35 r35 p31 r31 p29 p18 r29 r18 ");
        assert(!buttons.isButtonPressed(29) && !buttons.isButtonPressed(18));
        g_log.clear();
    }

    // --- keyboard events: mapped keys, repeat ignored, unmapped ignored, P, QUIT
    {
        SdlInput in(buttons, platform);
        pushKey(SDL_SCANCODE_RETURN, true);
        pushKey(SDL_SCANCODE_RETURN, true, true); // key repeat
        pushKey(SDL_SCANCODE_Q, true);            // not in the table
        in.pump(0);
        assert(g_log == "p33 " && buttons.isButtonPressed(33));
        pushKey(SDL_SCANCODE_RETURN, false);
        pushKey(SDL_SCANCODE_Q, false);
        pushKey(SDL_SCANCODE_P, true);
        in.pump(10);
        assert(g_log == "p33 r33 " && !buttons.isButtonPressed(33));
        assert(in.takeScreenshotRequest());
        assert(!in.takeScreenshotRequest());
        pushKey(SDL_SCANCODE_P, false);
        assert(!in.quitRequested());
        SDL_Event q;
        memset(&q, 0, sizeof(q));
        q.type = SDL_QUIT;
        SDL_PushEvent(&q);
        in.pump(20);
        assert(in.quitRequested());
        assert(!in.takeScreenshotRequest());
        g_log.clear();
    }

    // --- Escape: short tap does nothing, 500 ms hold sends 1 then 0 on release
    {
        SdlInput in(buttons, platform);
        pushKey(SDL_SCANCODE_ESCAPE, true);
        in.pump(1000);
        pushKey(SDL_SCANCODE_ESCAPE, false);
        in.pump(1400);
        assert(g_power.empty());
        pushKey(SDL_SCANCODE_ESCAPE, true);
        in.pump(3000);
        in.pump(3499);
        assert(g_power.empty());
        in.pump(3500);
        assert(g_power.size() == 1 && g_power[0] == 1);
        in.pump(6000);
        assert(g_power.size() == 1);
        pushKey(SDL_SCANCODE_ESCAPE, false);
        in.pump(6100);
        assert(g_power.size() == 2 && g_power[1] == 0);
        assert(g_log.empty()); // Escape never reaches ButtonsImpl
    }


    // --- mod column: L4 held changes the shoulders and Left/Right, L4 itself is the mod key
    {
        SdlInput in(buttons, platform);
        assert(!in.modHeld() && !strcmp(in.currentDevice().name, "brick"));
        in.onPhysical("f1", true, 0);   assert(g_log == "p46 " && in.modHeld());
        in.onPhysical("l1", true, 0);   assert(g_log == "p46 p24 ");          // copy
        in.onPhysical("f1", false, 0);  assert(g_log == "p46 p24 r46 " && !in.modHeld());
        in.onPhysical("l1", true, 0);   // repeat of the current state: ignored
        in.onPhysical("l1", false, 0);  assert(g_log == "p46 p24 r46 r24 "); // latched key released
        g_log.clear();
        in.onPhysical("f1", true, 0); in.onPhysical("l2", true, 0);
        assert(g_log == "p46 p29 p24 ");                                      // paste = Shift + Copy
        in.onPhysical("l2", false, 0);  assert(g_log == "p46 p29 p24 r24 r29 ");
        g_log.clear();
        in.onPhysical("f1", false, 0); g_log.clear();
        in.onPhysical("west", true, 0); in.onPhysical("f1", true, 0);        // Y held before L4 stays Shift
        in.onPhysical("l2", true, 0); in.onPhysical("l2", false, 0);
        assert(g_log == "p29 p46 p24 r24 ");                                  // user's Shift kept
        g_log.clear();
        in.onPhysical("dpad_up", true, 0); in.onPhysical("dpad_up", false, 0); // Shift + L4 + Up: Shift + Fn1
        assert(g_log == "p" + std::to_string(InputMap::miniIndex("fn1")) + " r" + std::to_string(InputMap::miniIndex("fn1")) + " ");
        in.onPhysical("west", false, 0); in.onPhysical("f1", false, 0);
        g_log.clear();
        in.onPhysical("f1", true, 0); g_log.clear();
        in.onPhysical("north", true, 0); in.onPhysical("north", false, 0);
        in.onPhysical("south", true, 0); in.onPhysical("south", false, 0);
        assert(g_log == "p48 r48 p49 r49 ");                                  // L4 + X undo, L4 + B redo
        in.onPhysical("f1", false, 0);
        // L4 + Up, then let go of L4 with Up still held: Fn1 stays held (Pattern-held chords) and the
        // shoulders are plain value keys again
        g_log.clear();
        const std::string fn1 = std::to_string(InputMap::miniIndex("fn1")), m1 = std::to_string(InputMap::miniIndex("minus1"));
        in.onPhysical("f1", true, 0); in.onPhysical("dpad_up", true, 0); in.onPhysical("f1", false, 0);
        in.onPhysical("l1", true, 0); in.onPhysical("l1", false, 0); in.onPhysical("dpad_up", false, 0);
        assert(g_log == "p46 p" + fn1 + " r46 p" + m1 + " r" + m1 + " r" + fn1 + " ");
        g_log.clear();
        // L4 + Menu: the screen scale toggle, a platform action; nothing reaches the firmware
        assert(!in.takeScaleToggle());
        in.onPhysical("f1", true, 0); g_log.clear();
        in.onPhysical("menu", true, 0); in.onPhysical("menu", false, 0);
        assert(g_log.empty() && in.takeScaleToggle() && !in.takeScaleToggle());
        in.onPhysical("f1", false, 0);
        in.onPhysical("menu", true, 0); in.onPhysical("menu", false, 0); // Menu alone is still Menu
        assert(!in.takeScaleToggle() && g_log.find("p") != std::string::npos);
        g_log.clear();
        in.onPhysical("dpad_left", true, 0); in.onPhysical("dpad_left", false, 0);
        assert(g_log == "p30 r30 ");                                           // no mod: Left
        in.onPhysical("f2", true, 0); in.onPhysical("f2", false, 0);
        assert(g_log == "p30 r30 p45 r45 ");                                   // R4 is the list key
        in.onPhysical("south", true, 0); in.onPhysical("south", false, 0);
        assert(g_log == "p30 r30 p45 r45 p33 r33 ");                           // B is plain Enter
        g_log.clear();

        // unbound under mod, A the column key, physical state
        in.onPhysical("f1", true, 0);
        in.onPhysical("east", true, 0);   // A: unbound under L4
        in.onPhysical("east", false, 0);
        in.onPhysical("f1", false, 0);
        in.onPhysical("east", true, 0);
        assert(in.isDown("east") && !in.isDown("nope"));
        in.onPhysical("east", false, 0);
        in.onPhysical("nope", true, 0);
        assert(g_log == P("mod") + R("mod") + P("column") + R("column") && !in.isDown("east"));
        g_log.clear();
    }

    // --- power chord: select + start within 150 ms of each other, held 500 ms from the
    // second press; neither reaches the firmware as Rec or Play
    {
        g_power.clear(); // the Escape block leaves its 1, 0
        SdlInput in(buttons, platform);
        in.onPhysical("select", true, 1000);
        in.pump(1100);
        in.onPhysical("start", true, 1149);
        in.pump(1648);
        assert(g_power.empty() && g_log.empty());
        in.pump(1649);
        assert(g_power.size() == 1 && g_power[0] == 1);
        in.pump(3000);
        assert(g_power.size() == 1);
        in.onPhysical("select", false, 3100);
        assert(g_power.size() == 2 && g_power[1] == 0);
        in.onPhysical("start", false, 3200);
        in.pump(5000);
        assert(g_power.size() == 2 && g_log.empty());
        // a release before 500 ms cancels, still with nothing delivered
        in.onPhysical("start", true, 8000);
        in.onPhysical("select", true, 8000);
        in.pump(8499);
        in.onPhysical("start", false, 8499);
        in.pump(20000);
        in.onPhysical("select", false, 20000);
        assert(g_power.size() == 2 && g_log.empty());
        g_power.clear();
    }

    // --- select and start alone: a tap delivers press and release at the release, a hold
    // delivers the press 150 ms late; a key the firmware already saw stays a normal key
    {
        SdlInput in(buttons, platform);
        in.onPhysical("select", true, 1000); // tap
        in.pump(1100);
        assert(g_log.empty());
        in.onPhysical("select", false, 1120);
        assert(g_log == "p28 r28 ");
        in.onPhysical("start", true, 2000); // hold
        in.pump(2149);
        assert(g_log == "p28 r28 ");
        in.pump(2150);
        assert(g_log == "p28 r28 p23 " && buttons.isButtonPressed(23));
        in.onPhysical("start", false, 2500);
        assert(g_log == "p28 r28 p23 r23 ");
        in.onPhysical("select", true, 3000); // held past the delay: Rec is already down
        in.pump(3200);
        in.onPhysical("start", true, 3300); // chord: Play is never delivered
        in.pump(3800);
        assert(g_power.size() == 1 && g_power[0] == 1);
        in.onPhysical("start", false, 3900);
        in.onPhysical("select", false, 3900);
        assert(g_power.size() == 2 && g_log == "p28 r28 p23 r23 p28 r28 ");
        g_power.clear();
        g_log.clear();
    }

    // --- script pad ids go through the mod column
    {
        SdlInput in(buttons, platform);
        bool ok = in.addScript("0:pad.f1+pad.l1,100:pad.south+up,200:pad.nope");
        assert(!ok);
        ok = in.addScript("0:pad.f1+pad.l1,100:pad.south+up");
        assert(ok);
        assert(!in.addScript("0:pad.") && !in.addScript("0:pad") && !in.addScript("0:padx.south"));
        for (uint32_t t = 0; t < 300; t += 10)
            in.pump(t);
        // ids joined with '+' release in press order: f1 (mod) first, then the latched copy
        assert(g_log == "p46 p24 r46 r24 " + P("enter") + P("up") + R("enter") + R("up"));
        g_log.clear();
    }

    // --- the SDL joystick path, through a virtual joystick laid out like the Pro
    r = SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    assert(r == 0);
    {
        SdlInput in(buttons, platform);
        bool ok = in.setDevice("brickpro") && !in.setDevice("nope");
        assert(ok);
        int devIndex = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 16, 1);
        assert(devIndex >= 0);
        in.pump(0); // SDL_JOYDEVICEADDED: SdlInput opens it
        SDL_Joystick* vj = SDL_JoystickOpen(devIndex); // same handle, refcounted
        assert(vj);
        SDL_JoystickSetVirtualButton(vj, 0, 1); // south: Enter
        in.pump(10);
        assert(g_log == P("enter"));
        SDL_JoystickSetVirtualButton(vj, 0, 0);
        in.pump(15);
        SDL_JoystickSetVirtualButton(vj, 11, 1); // f1 on the Pro
        in.pump(20);
        SDL_JoystickSetVirtualHat(vj, 0, SDL_HAT_LEFT);
        in.pump(30);
        SDL_JoystickSetVirtualButton(vj, 11, 0);
        in.pump(40);
        SDL_JoystickSetVirtualHat(vj, 0, SDL_HAT_CENTERED);
        SDL_JoystickSetVirtualAxis(vj, 2, 16000); // not above the threshold
        in.pump(50);
        SDL_JoystickSetVirtualAxis(vj, 2, 20000); // l2: minus10
        in.pump(60);
        assert(g_log == P("enter") + R("enter") + P("mod") + P("fn4") + R("mod") + R("fn4") + P("minus10") && buttons.isButtonPressed(InputMap::miniIndex("minus10"))); // f1 + left = Fn4

        // left stick: one press per push, hysteresis between kStickOn and kStickOff
        const std::string held = g_log;
        SDL_JoystickSetVirtualAxis(vj, 0, -25000); // ls_left: note
        in.pump(61);
        assert(g_log == held + P("note"));
        SDL_JoystickSetVirtualAxis(vj, 0, -15000); // under kStickOn, over kStickOff: still held
        in.pump(62);
        assert(g_log == held + P("note"));
        SDL_JoystickSetVirtualAxis(vj, 0, -10000);
        in.pump(63);
        assert(g_log == held + P("note") + R("note"));
        SDL_JoystickSetVirtualAxis(vj, 0, 15000); // not past kStickOn
        in.pump(64);
        SDL_JoystickSetVirtualAxis(vj, 1, 25000); // ls_down: fx2
        in.pump(65);
        SDL_JoystickSetVirtualAxis(vj, 1, 0);
        SDL_JoystickSetVirtualAxis(vj, 0, 0);
        in.pump(66);
        assert(g_log == held + P("note") + R("note") + P("fx2") + R("fx2"));
        g_log = held;
        printf("left stick: column keys with hysteresis\n");

        // right stick as the jog wheel: kWheelDetents steps per turn reach onPotChange(0, +-1)
        in.setPotCallback(onPot);
        auto wheel = [&](double turns, int radius) {
            const double a = turns * 2 * M_PI;
            SDL_JoystickSetVirtualAxis(vj, 3, (Sint16)lround(radius * cos(a)));
            SDL_JoystickSetVirtualAxis(vj, 4, (Sint16)lround(radius * sin(a)));
            in.pump(67);
        };
        const int n = InputMap::kWheelDetents;
        wheel(0, 0); // the virtual pad starts axis 4 at -32768 (a trigger in SDL's layout)
        g_pots.clear();
        for (int k = 0; k <= n; k++) // clockwise (SDL y points down), through the +-pi seam
            wheel((double)k / n, 25000);
        assert(g_pots == std::vector<int>(n, +1));
        g_pots.clear();
        for (int k = n; k >= 0; k--)
            wheel((double)k / n, 25000);
        assert(g_pots == std::vector<int>(n, -1));
        g_pots.clear();
        wheel(0, 0); // back to rest
        for (int j : { 500, -500, 300, -300 })
        {
            SDL_JoystickSetVirtualAxis(vj, 3, (Sint16)j);
            SDL_JoystickSetVirtualAxis(vj, 4, (Sint16)-j);
            in.pump(68);
        }
        assert(g_pots.empty() && g_log == held);
        in.setPotCallback(nullptr);
        printf("right stick: %d detents per turn, both ways, rest jitter ignored\n", n);

        SDL_JoystickClose(vj);
        SDL_JoystickDetachVirtual(devIndex); // removal releases what is still held
        in.pump(70);
        assert(g_log == P("enter") + R("enter") + P("mod") + P("fn4") + R("mod") + R("fn4") + P("minus10") + R("minus10") && !buttons.isButtonPressed(InputMap::miniIndex("minus10")));
        printf("virtual joystick: button, hat, axis and removal events reached onPhysical\n");
        g_log.clear();
    }

    SDL_Quit();
    printf("input_test: all checks passed (%zu mini ids, %zu desktop keys)\n",
           InputMap::kMiniKeyCount, InputMap::kDesktopKeyCount);
    return 0;
}
