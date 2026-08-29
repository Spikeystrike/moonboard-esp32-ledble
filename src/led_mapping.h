#pragma once

#include <stddef.h>
#include <stdint.h>

bool logicalToPhysicalLed(
    uint16_t logicalPosition,
    const uint16_t *mapping,
    size_t mappingCount,
    uint16_t physicalLedCount,
    uint16_t &physicalPosition);

bool validateLedMapping(
    const uint16_t *mapping,
    size_t mappingCount,
    uint16_t physicalLedCount);
