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
