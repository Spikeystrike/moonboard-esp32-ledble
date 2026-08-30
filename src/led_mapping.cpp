#include "led_mapping.h"

bool logicalToPhysicalLed(
    uint16_t logicalPosition,
    const uint16_t *mapping,
    size_t mappingCount,
    uint16_t physicalLedCount,
    uint16_t &physicalPosition)
{
    if (mapping == nullptr || logicalPosition >= mappingCount)
        return false;

    const uint16_t mappedPosition = mapping[logicalPosition];
    if (mappedPosition >= physicalLedCount)
        return false;

    physicalPosition = mappedPosition;
    return true;
}

bool validateLedMapping(
    const uint16_t *mapping,
    size_t mappingCount,
    uint16_t physicalLedCount)
{
    if (mapping == nullptr || mappingCount == 0 || physicalLedCount == 0)
        return false;

    for (size_t index = 0; index < mappingCount; ++index)
    {
        if (mapping[index] >= physicalLedCount)
            return false;

        for (size_t previous = 0; previous < index; ++previous)
        {
            if (mapping[previous] == mapping[index])
                return false;
        }
    }

    return true;
}

bool validateUnmappedPhysicalLeds(
    const uint16_t *physicalLeds,
    size_t physicalLedCount,
    uint16_t totalPhysicalLedCount,
    const uint16_t *logicalMapping,
    size_t logicalMappingCount)
{
    if (physicalLedCount == 0)
        return true;
    if (
        physicalLeds == nullptr ||
        totalPhysicalLedCount == 0 ||
        (logicalMapping == nullptr && logicalMappingCount > 0))
    {
        return false;
    }

    for (size_t index = 0; index < physicalLedCount; ++index)
    {
        const uint16_t physicalLed = physicalLeds[index];
        if (physicalLed >= totalPhysicalLedCount)
            return false;

        for (size_t previous = 0; previous < index; ++previous)
        {
            if (physicalLeds[previous] == physicalLed)
                return false;
        }

        for (size_t logical = 0; logical < logicalMappingCount; ++logical)
        {
            if (logicalMapping[logical] == physicalLed)
                return false;
        }
    }

    return true;
}
