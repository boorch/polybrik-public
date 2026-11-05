#include "CrcCalculatorImpl.h"

uint32_t CrcCalculatorImpl::calculate32bitCrc(const uint8_t* data, const uint16_t datalen)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint16_t i = 0; i < datalen; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
