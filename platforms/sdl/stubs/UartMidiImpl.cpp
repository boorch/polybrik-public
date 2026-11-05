#include "UartMidiImpl.h"

bool UartMidiImpl::isReadAvailable() { return false; }
void UartMidiImpl::read() {}
void UartMidiImpl::sendRealTime(MessageType) {}
void UartMidiImpl::sendControlChange(uint8_t, uint8_t, uint8_t) {}
void UartMidiImpl::sendProgramChange(uint8_t, uint8_t) {}
void UartMidiImpl::sendAfterTouch(uint8_t, uint8_t) {}
void UartMidiImpl::sendPolyPressure(uint8_t, uint8_t, uint8_t) {}
void UartMidiImpl::sendPitchBend(int, uint8_t) {}
void UartMidiImpl::sendNoteOn(uint8_t, uint8_t, uint8_t) {}
void UartMidiImpl::sendNoteOff(uint8_t, uint8_t, uint8_t) {}
void UartMidiImpl::init() {}
void UartMidiImpl::setReceiveActions(MidiReceiveActions* actions)
{
    if (actions)
        receiveActions = *actions;
}
void UartMidiImpl::setReceiveNoteOnAction(void (*action)(uint8_t, uint8_t, uint8_t))
{
    receiveActions.noteOn = action;
}
