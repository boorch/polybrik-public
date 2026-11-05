// Entry point.
#include "InputMap.h"
#include "NullDisplayDriver.h"
#include "PlatformImpl.h"
#include "Tracker.h"
#include "eve/Canvas.h"
#include "eve/EveDisplayDriver.h"
#include "eve/EveEmu.h"
#include "io/AudioHost.h"
#include "io/IAudioOut.h"
#include "io/Legend.h"
#include "io/PageId.h"
#include "core/sdlActionList.h"
#include "SimClock.h"
#include "io/Png.h"
#include "io/WavAudioOut.h"
#include "io/sdl/SdlAudioOut.h"
#ifdef POLYBRIK_FBDEV
#include "io/raw/FbdevDisplayOut.h"
#else
#include "io/sdl/SdlDisplayOut.h"
#endif
#include "io/sdl/SdlInput.h"
#include "mtStructs.h"

#include <SDL.h>
#ifdef POLYBRIK_T32
#include <csignal>
#include <ucontext.h>
#include <unistd.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Core input entry points (core/source/mtActions.cpp), declared as platforms/imx/source/hardware.cpp does.
void onPotChange(uint8_t n, int16_t value);
void onButtonPush(uint8_t n);
void onButtonRelease(uint8_t n);
void onButtonHold(uint8_t n);

namespace
{
struct Options
{
    const char* sd = nullptr;
    bool headless = false;
    bool noWindow = false;    // render and screenshot without opening a window (golden runs)
    bool genericR4 = false;   // R4 = screen-key mode on every page, curated lists ignored (debugging)
    bool simClock = false;    // simulated firmware time, 1 ms per loop iteration (golden runs)
    bool fromReady = false;   // --keys, --screenshot-at and --quit-after count from the first settled firmware page, not from launch
    bool fullscreen = false;
    int scale = 0;
    uint64_t quitAfterMs = 0;
    const char* log = nullptr;
    const char* screenshot = nullptr;
    uint64_t screenshotAtMs = 0;
    bool screenshotAt = false;
    bool dumpDl = false;
    const char* keys = nullptr;
    const char* audioFile = nullptr;
    bool noAudio = false;
    const char* device = nullptr;
    bool noLegend = false;
    const char* screenshotStrip = nullptr;
};

void usage(const char* argv0)
{
    fprintf(stderr,
            "usage: %s --sd DIR [--headless] [--fullscreen] [--scale N | --no-scale] [--quit-after MS] [--log FILE]\n"
            "          [--screenshot PATH] [--screenshot-at MS] [--dump-dl] [--keys SPEC]\n"
            "          [--audio-file PATH | --no-audio] [--device brick|brickpro] [--no-legend]\n"
            "          [--screenshot-strip PATH] [--no-window] [--from-ready] [--sim-clock]\n          [--generic-r4]\n",
            argv0);
    exit(2);
}

// The audio out is stopped from main's quit path and from atexit: PlatformImpl::setPowerMode(false)
// calls exit(0) from inside tracker->update(), and the WAV must be finalised there too.
AudioHost* g_audioHost = nullptr;
std::unique_ptr<IAudioOut> g_audioOut;

void stopAudio()
{
    if (!g_audioOut)
        return;
    g_audioOut->stop();
    fprintf(stderr, "audio: %llu blocks, max render %u us, last render %u us, max lock wait %u us\n",
            (unsigned long long)g_audioHost->blocks(), g_audioHost->maxRenderUs(),
            g_audioHost->lastRenderUs(), g_audioHost->maxLockWaitUs());
    g_audioOut->printStats(stderr);
    g_audioOut.reset();
}

Options parseArgs(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; i++)
    {
        bool hasValue = i + 1 < argc;
        if (!strcmp(argv[i], "--sd") && hasValue)
            o.sd = argv[++i];
        else if (!strcmp(argv[i], "--headless"))
            o.headless = true;
        else if (!strcmp(argv[i], "--no-window"))
            o.noWindow = true;
        else if (!strcmp(argv[i], "--from-ready"))
            o.fromReady = true;
        else if (!strcmp(argv[i], "--sim-clock"))
            o.simClock = true;
        else if (!strcmp(argv[i], "--generic-r4"))
            o.genericR4 = true;
        else if (!strcmp(argv[i], "--fullscreen"))
            o.fullscreen = true;
        else if (!strcmp(argv[i], "--scale") && hasValue)
            o.scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--no-scale"))
            o.scale = 1;   // the 800x480 canvas 1:1, centred, no stretch to the screen width
        else if (!strcmp(argv[i], "--quit-after") && hasValue)
            o.quitAfterMs = strtoull(argv[++i], nullptr, 10);
        else if (!strcmp(argv[i], "--log") && hasValue)
            o.log = argv[++i];
        else if (!strcmp(argv[i], "--screenshot") && hasValue)
            o.screenshot = argv[++i];
        else if (!strcmp(argv[i], "--screenshot-at") && hasValue)
        {
            o.screenshotAt = true;
            o.screenshotAtMs = strtoull(argv[++i], nullptr, 10);
        }
        else if (!strcmp(argv[i], "--dump-dl"))
            o.dumpDl = true;
        else if (!strcmp(argv[i], "--keys") && hasValue)
            o.keys = argv[++i];
        else if (!strcmp(argv[i], "--audio-file") && hasValue)
            o.audioFile = argv[++i];
        else if (!strcmp(argv[i], "--no-audio"))
            o.noAudio = true;
        else if (!strcmp(argv[i], "--device") && hasValue)
            o.device = argv[++i];
        else if (!strcmp(argv[i], "--no-legend"))
            o.noLegend = true;
        else if (!strcmp(argv[i], "--screenshot-strip") && hasValue)
            o.screenshotStrip = argv[++i];
        else
            usage(argv[0]);
    }
    if (!o.sd || (o.audioFile && o.noAudio))
        usage(argv[0]);
    return o;
}

// --screenshot PATH, or screenshot-<ms>.png next to the sd dir.
void screenshot(const Options& opt, const Canvas& canvas, uint64_t ms)
{
    std::string path;
    if (opt.screenshot)
        path = opt.screenshot;
    else
    {
        const char* slash = strrchr(opt.sd, '/');
        path = slash ? std::string(opt.sd, (size_t)(slash - opt.sd + 1)) : std::string();
        path += "screenshot-" + std::to_string(ms) + ".png";
    }
    fprintf(stderr, "screenshot %s at %llu ms: %s\n", path.c_str(), (unsigned long long)ms,
            savePng(canvas, path.c_str()) ? "ok" : "FAILED");
}

// --device, else DEVICE from the environment when it names a known map, else "brick".
const char* deviceName(const Options& opt)
{
    if (opt.device)
        return opt.device;
    const char* env = getenv("DEVICE");
    return env && InputMap::brickDevice(env) ? env : "brick";
}
#ifdef POLYBRIK_T32
// T32 runs on the device with no debugger: log the fault address and pc/lr (map them with nm on the
// unstripped build-t32app/app/tracker-sdl), then die with the same signal.
void crashHandler(int sig, siginfo_t* si, void* ctx)
{
    const mcontext_t& m = static_cast<ucontext_t*>(ctx)->uc_mcontext;
    char buf[160];
    int n = snprintf(buf, sizeof buf, "FATAL signal %d addr %p pc %08lx lr %08lx sp %08lx\n", sig, si->si_addr,
                     (unsigned long)m.arm_pc, (unsigned long)m.arm_lr, (unsigned long)m.arm_sp);
    if (write(2, buf, (size_t)n) < 0) {}
    signal(sig, SIG_DFL);
    raise(sig);
}

void installCrashHandler()
{
    struct sigaction sa{};
    sa.sa_sigaction = crashHandler;
    sa.sa_flags = SA_SIGINFO;
    for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
        sigaction(sig, &sa, nullptr);
}
#endif
} // namespace

int main(int argc, char** argv)
{
    Options opt = parseArgs(argc, argv);
#ifdef POLYBRIK_T32
    installCrashHandler();
#endif
    if (opt.log && !freopen(opt.log, "w", stderr))
    {
        perror(opt.log);
        return 1;
    }
    uint32_t sdlFlags = SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK;
#ifndef POLYBRIK_FBDEV   // T32: the framebuffer presenter needs no SDL video
    if (!opt.headless && !opt.noWindow)
        sdlFlags |= SDL_INIT_VIDEO;
#endif
    if (!opt.noAudio && !opt.audioFile)
        sdlFlags |= SDL_INIT_AUDIO;
    if (SDL_Init(sdlFlags) != 0)
    {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SimClock::on = opt.simClock;
    sdlActionList.setGenericOnly(opt.genericR4);
    auto nowMs = [] { return SimClock::on ? SimClock::us / 1000u : SDL_GetTicks64(); };
    const uint64_t t0 = nowMs();

    Canvas canvas(800, 480);
    EveEmu eve(canvas);
    if (opt.dumpDl)
        eve.setDumpStream(stderr);
    EveDisplayDriver eveDriver(eve);
    NullDisplayDriver nullDriver;   // --headless: the canvas stays black
    std::unique_ptr<IDisplayOut> out;
    if (!opt.headless && !opt.noWindow)
#ifdef POLYBRIK_FBDEV
        out.reset(new FbdevDisplayOut("/dev/fb0", opt.scale == 1));   // --scale 1 / --no-scale: 1:1; --fullscreen does not apply
#else
        out.reset(new SdlDisplayOut(opt.scale, opt.fullscreen));
#endif
    bool unscaled = opt.scale == 1;   // L4 + Menu toggles it
    PlatformImpl platform(opt.sd, opt.headless ? static_cast<DisplayDriver*>(&nullDriver) : &eveDriver);

    Tracker* tracker = Tracker::getInstance();
    eveDriver.init();
    nullDriver.init();
    tracker->getStartDisplayFunc()(&platform);
    // Audio starts before platform.init(); the firmware guards on its own state.
    // getCodecAudio() returns PlatformImpl's Sgtl5000Impl member, so the downcast is exact.
    AudioHost audioHost(tracker, static_cast<const Sgtl5000Impl*>(platform.getCodecAudio()));
    g_audioHost = &audioHost;
    if (!opt.noAudio)
    {
        if (opt.audioFile)
            g_audioOut.reset(new WavAudioOut(audioHost, opt.audioFile));
        else
            g_audioOut.reset(new SdlAudioOut(audioHost));
        if (!g_audioOut->start())
            return 1;
        atexit(stopAudio);
    }
    platform.init();
    // hardware.cpp:99-101: the platform registers the core's button callbacks itself.
    platform.buttonsImpl().setActionCallback(SwitchGroup::kPress, onButtonPush);
    platform.buttonsImpl().setActionCallback(SwitchGroup::kRelease, onButtonRelease);
    platform.buttonsImpl().setActionCallback(SwitchGroup::kHold, onButtonHold);
    tracker->init(&platform);

    SdlInput input(platform.buttonsImpl(), platform);
    input.setPotCallback(onPotChange); // Pro right stick jog wheel, as the IMX encoder (hardware.cpp:120)
    if (opt.keys && !input.addScript(opt.keys))
        usage(argv[0]);
    const char* dev = deviceName(opt);
    if (!input.setDevice(dev))
        usage(argv[0]);
    fprintf(stderr, "input: device %s\n", dev);
    // The legend lists only inputs the device has (no L3/R3 cells filled on the plain Brick).
    std::vector<BrickBinding> legendRows;
    for (size_t i = 0; i < InputMap::kBrickBindingCount; i++)
        if (InputMap::joySource(input.currentDevice(), InputMap::kBrickBindings[i].input))
            legendRows.push_back(InputMap::kBrickBindings[i]);
    Legend legend;
    legend.setStickKeys(InputMap::joySource(input.currentDevice(), "l3") != nullptr);   // no L3 / R3 cells on the plain Brick
    const bool showLegend = !opt.noLegend;
    bool lastMod = false, lastList = false;
    const char* lastPage = "";
    uint32_t lastHeld = 0;

    bool shotDone = false;
    // Boot time varies with machine load; --from-ready moves the script clock to the first settled firmware page.
    bool ready = !opt.fromReady;
    uint64_t base = t0;
    int lastBrightness = -1;
    bool lastPower = true;
    for (;;)
    {
        if (SimClock::on)
            SimClock::us = (SimClock::us / 1000u + 1u) * 1000u;
        const uint64_t now = nowMs();
        input.pump((uint32_t)now, ready);
        if (input.quitRequested())
            break;
        platform.update();
        tracker->update();
        if (!ready && firmwarePageSettled())
        {
            ready = true;
            base = nowMs();
            fprintf(stderr, "ready: page settled at %llu ms\n", (unsigned long long)(base - t0));
        }
        else if (!ready && nowMs() - t0 > 30000)
        {
            fprintf(stderr, "--from-ready: no settled firmware page after 30 s\n");
            return 1;
        }
        platform.buttonsImpl().tick((uint32_t)now);

        if (out)
        {
            if (eveDriver.brightness() != lastBrightness)
                out->setBrightness(lastBrightness = eveDriver.brightness());
            if (eveDriver.powerState() != lastPower)
                out->setBlank(!(lastPower = eveDriver.powerState()));
        }
        // The mod key and the list can change between firmware frames, so a change presents too.
        const bool mod = input.modHeld(), list = listVisible();
        const char* page = currentPageId();
        uint32_t held = 0;
        for (size_t i = 0; i < legendRows.size() && i < 32; i++)
            if (input.isDown(legendRows[i].input)) held |= 1u << i;
        const bool frame = eve.takeFrame();
        if (frame || mod != lastMod || list != lastList || held != lastHeld || strcmp(page, lastPage))
        {
            lastMod = mod;
            lastList = list;
            lastPage = page;
            lastHeld = held;
            if (showLegend)
                legend.render(eve, legendRows.data(), legendRows.size(), mod, page, held, list);
            if (out)
            {
                if (showLegend)
                    out->presentStrip(legend.pixels(), Legend::W, Legend::H);
                out->present(canvas);
            }
        }
        if (!frame && !SimClock::on)
            SDL_Delay(1);

        const uint64_t elapsed = nowMs() - base;
        sdlActionList.idleTick((uint32_t)nowMs());   // closed list: keep the dark cells' values live
        if (input.takeScaleToggle() && out)   // L4 + Menu
        {
            unscaled = !unscaled;
            out->setUnscaled(unscaled);
        }
        bool shot = input.takeScreenshotRequest();   // P key
        if (ready && !shotDone && opt.screenshotAt && elapsed >= opt.screenshotAtMs)
            shot = shotDone = true;
        if (shot)
        {
            screenshot(opt, canvas, elapsed);
            if (opt.screenshotStrip)
                fprintf(stderr, "strip screenshot %s: %s\n", opt.screenshotStrip,
                        savePng(legend.canvas(), opt.screenshotStrip) ? "ok" : "FAILED");
        }
        if (ready && opt.quitAfterMs && elapsed >= opt.quitAfterMs)
            break;
    }

    stopAudio();
    out.reset();
    SDL_Quit();
    return 0;
}
