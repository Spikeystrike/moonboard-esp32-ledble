#pragma once

#include <stdint.h>

struct RgbColor
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    RgbColor() : red(0), green(0), blue(0) {}

    RgbColor(uint8_t redValue, uint8_t greenValue, uint8_t blueValue)
        : red(redValue), green(greenValue), blue(blueValue)
    {
    }

    bool isBlack() const
    {
        return red == 0 && green == 0 && blue == 0;
    }
};

struct WledControllerConfig
{
    const char *host;
    uint16_t firstGlobalLed;
    uint16_t lastGlobalLed;
    uint8_t segmentId;
};
