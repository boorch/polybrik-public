#ifndef PLATFORMS_SDL_STUBS_UARTMIDIIMPL_H_
#define PLATFORMS_SDL_STUBS_UARTMIDIIMPL_H_

#include "Platform.h"

// Stub until section 10. Actions are copied for the same reason as UsbMidiImpl.
class UartMidiImpl final : public UartMidi
{
  public:
    bool isReadAvailable() override;
    void read() override;
    void sendRealTime(MessageType messageType) override;
    void sendControlChange(uint8_t index, uint8_t value, uint8_t channel) override;
    void sendProgramChange(uint8_t programNumber, uint8_t channel) override;
    void sendAfterTouch(uint8_t velocity, uint8_t channel) override;
    void sendPolyPressure(uint8_t note, uint8_t value, uint8_t channel) override;
    void sendPitchBend(int value, uint8_t channel) override;
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) override;
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) override;
    void init() override;
    void setReceiveActions(MidiReceiveActions* receiveActions) override;
    void setReceiveNoteOnAction(void (*action)(uint8_t, uint8_t, uint8_t)) override;

  private:
    MidiReceiveActions receiveActions = {};
};

#endif
