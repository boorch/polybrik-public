#ifndef PLATFORMS_SDL_STUBS_USBMIDIIMPL_H_
#define PLATFORMS_SDL_STUBS_USBMIDIIMPL_H_

#include "Platform.h"

// Stub until section 10. setReceiveActions copies: core passes a stack local.
class UsbMidiImpl final : public UsbMidi
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
    void setReceiveActions(MidiReceiveActions* receiveActions) override;

  private:
    MidiReceiveActions receiveActions = {};
};

#endif
