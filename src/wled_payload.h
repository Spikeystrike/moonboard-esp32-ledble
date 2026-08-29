#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "bridge_types.h"

std::string buildWledStatePayload(
    const RgbColor *leds,
    size_t ledCount,
    const WledControllerConfig &controller,
    uint8_t brightnessPercent,
    size_t &litLedCount);
