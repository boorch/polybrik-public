#include "UsbMidiImpl.h"

bool UsbMidiImpl::isReadAvailable() { return false; }
void UsbMidiImpl::read() {}
void UsbMidiImpl::sendRealTime(MessageType) {}
void UsbMidiImpl::sendControlChange(uint8_t, uint8_t, uint8_t) {}
void UsbMidiImpl::sendProgramChange(uint8_t, uint8_t) {}
void UsbMidiImpl::sendAfterTouch(uint8_t, uint8_t) {}
void UsbMidiImpl::sendPolyPressure(uint8_t, uint8_t, uint8_t) {}
void UsbMidiImpl::sendPitchBend(int, uint8_t) {}
void UsbMidiImpl::sendNoteOn(uint8_t, uint8_t, uint8_t) {}
void UsbMidiImpl::sendNoteOff(uint8_t, uint8_t, uint8_t) {}
void UsbMidiImpl::setReceiveActions(MidiReceiveActions* actions)
{
    if (actions)
        receiveActions = *actions;
}
