#include "UsbImpl.h"

void UsbImpl::init() {}
void UsbImpl::reset() {}
void UsbImpl::update() {}
void UsbImpl::setClassState(ClassType type, bool enable) { state[(int)type] = enable; }
bool UsbImpl::getClassState(ClassType type) { return state[(int)type]; }
bool UsbImpl::isAttached() { return false; }
