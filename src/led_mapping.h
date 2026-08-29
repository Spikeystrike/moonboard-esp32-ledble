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

bool validateUnmappedPhysicalLeds(
    const uint16_t *physicalLeds,
    size_t physicalLedCount,
    uint16_t totalPhysicalLedCount,
    const uint16_t *logicalMapping,
    size_t logicalMappingCount);
