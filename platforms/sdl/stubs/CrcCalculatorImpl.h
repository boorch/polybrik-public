#ifndef PLATFORMS_SDL_STUBS_CRCCALCULATORIMPL_H_
#define PLATFORMS_SDL_STUBS_CRCCALCULATORIMPL_H_

#include "Platform.h"

// CRC-32 (poly 0x04C11DB7 reflected, init and xorout 0xFFFFFFFF), the FastCRC32::crc32 that
// platforms/mk66/source/CrcCalculatorImpl.cpp calls. Check value for "123456789" is 0xCBF43926.
class CrcCalculatorImpl final : public CrcCalculator
{
  public:
    uint32_t calculate32bitCrc(const uint8_t* data, const uint16_t datalen) override;
};

#endif
