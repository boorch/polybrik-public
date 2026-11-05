#include "NullDisplayDriver.h"

#include <cstring>

void NullDisplayDriver::getImgProps(uint32_t& addr, uint32_t& width, uint32_t& height)
{
    addr = width = height = 0;
}

void NullDisplayDriver::readDataRAMG(uint8_t* imgData, uint32_t dataSize, uint32_t)
{
    memset(imgData, 0, dataSize);
}

void NullDisplayDriver::readBackDataRAMG(uint8_t* imgData, uint32_t dataSize, uint32_t)
{
    memset(imgData, 0, dataSize);
}

// Byte count API_SendString (platforms/imx/3rdparty/FT812/API_Layer.cpp) reports:
// string plus terminating null, padded to a multiple of 4.
uint8_t NullDisplayDriver::sendString(const char* string, uint32_t length)
{
    uint32_t rl = strlen(string);
    if (length == 0 || length > rl)
        length = rl;
    return (uint8_t)((length + 1 + 3) & ~3u);
}
