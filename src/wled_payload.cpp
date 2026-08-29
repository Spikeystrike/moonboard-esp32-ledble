#include "wled_payload.h"

#include <algorithm>
#include <cstdio>

namespace
{
uint8_t scaleChannel(uint8_t value, uint8_t brightnessPercent)
{
    const uint16_t percent = std::min<uint16_t>(brightnessPercent, 100);
    return static_cast<uint8_t>((value * percent + 50) / 100);
}
} // namespace

std::string buildWledStatePayload(
    const RgbColor *leds,
    size_t ledCount,
    const WledControllerConfig &controller,
    uint8_t brightnessPercent,
    size_t &litLedCount)
{
    litLedCount = 0;
    if (
        leds == nullptr ||
        ledCount == 0 ||
        controller.lastGlobalLed < controller.firstGlobalLed ||
        controller.firstGlobalLed >= ledCount)
    {
        return std::string();
    }

    const size_t lastLed = std::min<size_t>(
        controller.lastGlobalLed,
        ledCount - 1);
    std::string pixels;
    pixels.reserve((lastLed - controller.firstGlobalLed + 1) * 14);

    char item[32];
    for (
        size_t globalLed = controller.firstGlobalLed;
        globalLed <= lastLed;
        ++globalLed)
    {
        const RgbColor &color = leds[globalLed];
        if (color.isBlack())
            continue;

        const uint16_t localLed = static_cast<uint16_t>(
            globalLed - controller.firstGlobalLed);
        std::snprintf(
            item,
            sizeof(item),
            "%s%u,\"%02X%02X%02X\"",
            litLedCount == 0 ? "" : ",",
            localLed,
            scaleChannel(color.red, brightnessPercent),
            scaleChannel(color.green, brightnessPercent),
            scaleChannel(color.blue, brightnessPercent));
        pixels += item;
        ++litLedCount;
    }

    if (litLedCount == 0)
        return std::string();

    char prefix[96];
    std::snprintf(
        prefix,
        sizeof(prefix),
        "{\"on\":true,\"bri\":255,\"seg\":{\"id\":%u,\"fx\":0,\"i\":[",
        controller.segmentId);
    return std::string(prefix) + pixels + "]}}";
}
