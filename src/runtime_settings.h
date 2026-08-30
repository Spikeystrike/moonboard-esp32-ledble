#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "bridge_types.h"

constexpr size_t MAX_LOGICAL_LED_COUNT = 200;
constexpr size_t MAX_KICKER_LED_COUNT = 64;
constexpr size_t MAX_WLED_HOST_LENGTH = 64;
constexpr uint16_t MAX_PHYSICAL_LED_COUNT = 2048;

struct RuntimeSettings
{
    char wledHost[MAX_WLED_HOST_LENGTH];
    uint16_t physicalLedCount;
    uint8_t segmentId;
    uint8_t boulderBrightnessPercent;
    uint8_t aboveHoldBrightnessPercent;
    uint16_t routeTimeoutMinutes;
    bool kickerLedsEnabled;
    RgbColor kickerLedColor;
    uint16_t logicalMapping[MAX_LOGICAL_LED_COUNT];
    uint16_t logicalMappingCount;
    uint16_t kickerLedIds[MAX_KICKER_LED_COUNT];
    uint16_t kickerLedCount;
};

bool validateRuntimeSettings(
    const RuntimeSettings &settings,
    std::string &error);

bool parseLedIdList(
    const std::string &text,
    uint16_t *output,
    size_t outputCapacity,
    uint16_t &outputCount,
    std::string &error);
