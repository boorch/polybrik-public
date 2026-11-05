#include "BatteryImpl.h"

void BatteryImpl::enableMonitoring() {}
int BatteryImpl::getLevel() { return 100; }
float BatteryImpl::getVoltage() { return 4.1f; }
bool BatteryImpl::getCharging() { return false; }
int BatteryImpl::getTimeTo() { return 0; }
bool BatteryImpl::getState() { return true; }
bool BatteryImpl::isEmpty() { return false; }
