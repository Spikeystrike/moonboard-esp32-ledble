#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "bridge_types.h"

class WledClient
{
public:
    WledClient(
        const WledControllerConfig *controllers,
        size_t controllerCount,
        uint16_t timeoutMs);

    bool validateConfiguration() const;
    bool reset();
    bool render(
        const RgbColor *leds,
        size_t ledCount,
        uint8_t brightnessPercent);

private:
    bool postJson(
        const WledControllerConfig &controller,
        const char *payload) const;
    bool postJson(
        const WledControllerConfig &controller,
        const std::string &payload) const;

    const WledControllerConfig *controllers_;
    size_t controllerCount_;
    uint16_t timeoutMs_;
};
