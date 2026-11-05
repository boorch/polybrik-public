#ifndef PLATFORMS_SDL_PLATFORMIMPL_H_
#define PLATFORMS_SDL_PLATFORMIMPL_H_

// Mirrors tracker-ce/platforms/imx/source/PlatformImpl.h: every Impl is a member.
#include "ButtonsImpl.h"
#include "ConfigManagerImpl.h"
#include "FileSystemImpl.h"
#include "DisplayDriver.h"
#include "Platform.h"
#include "TimerImpl.h"
#include "stubs/BatteryImpl.h"
#include "stubs/CrcCalculatorImpl.h"
#include "stubs/EncoderImpl.h"
#include "stubs/LedsImpl.h"
#include "stubs/PadsImpl.h"
#include "stubs/RadioImpl.h"
#include "stubs/Sgtl5000Impl.h"
#include "stubs/UartMidiImpl.h"
#include "stubs/UsbAudioImpl.h"
#include "stubs/UsbImpl.h"
#include "stubs/UsbMidiImpl.h"

class PlatformImpl final : public Platform
{
  public:
    // The display driver is owned by main (EveDisplayDriver, or NullDisplayDriver with --headless).
    PlatformImpl(const char* sdRoot, DisplayDriver* displayDriver);

    void setPowerMode(bool mode) override;
    void resetMCU() override;
    void runBootloader(char* fileName) override;
    void setPowerButtonCallback(PowerButtonCallback) override;
    bool getPowerButtonState() override;
    bool readForceStartUp() override;

    Sgtl5000* getCodecAudio() override { return &codec; }
    Radio* getRadio() override { return &radio; }
    SwitchGroup* getButtons() override { return &buttons; }
    SwitchGroup* getPads() override { return &pads; }
    Leds* getLeds() override { return &leds; }
    Battery* getBattery() override { return &battery; }
    Timer* getTimer() override { return &timer; }
    Encoder* getEncoder() override { return &encoder; }
    DisplayDriver* getDisplayDriver() override { return displayDriver; }
    UsbAudio* getUsbAudio() override { return &usbAudio; }
    UartMidi* getUartMidi() override { return &uartMidi; }
    UsbMidi* getUsbMidi() override { return &usbMidi; }
    ConfigManager* getConfigManager() override { return &configManager; }
    FileSystem* getFileSystem() override { return &fileSystem; }
    CrcCalculator* getCrcCalculator() override { return &crcCalculator; }
    SystemInfo* getSystemInfo() override { return &systemInfo; }
    Usb* getUsb() override { return &usb; }

    void init();
    void update();

    ButtonsImpl& buttonsImpl() { return buttons; }
    PowerButtonCallback getPowerButtonCallback() { return powerButtonCallback; }

  private:
    PowerButtonCallback powerButtonCallback = nullptr;
    DisplayDriver* displayDriver;
    TimerImpl timer;
    Sgtl5000Impl codec;
    ButtonsImpl buttons;
    PadsImpl pads;
    CrcCalculatorImpl crcCalculator;
    BatteryImpl battery;
    ConfigManagerImpl configManager;
    UsbAudioImpl usbAudio;
    UsbMidiImpl usbMidi;
    UartMidiImpl uartMidi;
    FileSystemImpl fileSystem;
    LedsImpl leds;
    EncoderImpl encoder;
    RadioImpl radio;
    UsbImpl usb;

    static SystemInfo systemInfo;
};

#endif
