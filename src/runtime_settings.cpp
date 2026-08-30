#include "runtime_settings.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace
{
bool validHost(const char *host)
{
    if (host == nullptr || host[0] == '\0')
        return false;

    const size_t length = std::strlen(host);
    if (length >= MAX_WLED_HOST_LENGTH)
        return false;

    for (size_t index = 0; index < length; ++index)
    {
        const unsigned char value = static_cast<unsigned char>(host[index]);
        if (value <= 32 || value >= 127 || host[index] == '"')
            return false;
    }
    return true;
}
} // namespace

bool validateRuntimeSettings(
    const RuntimeSettings &settings,
    std::string &error)
{
    if (!validHost(settings.wledHost))
    {
        error = "WLED host is empty or contains invalid characters";
        return false;
    }
    if (
        settings.physicalLedCount == 0 ||
        settings.physicalLedCount > MAX_PHYSICAL_LED_COUNT)
    {
        error = "Physical LED count is outside the supported range";
        return false;
    }
    if (
        settings.logicalMappingCount == 0 ||
        settings.logicalMappingCount > MAX_LOGICAL_LED_COUNT)
    {
        error = "Logical LED count is outside the supported range";
        return false;
    }
    if (
        settings.boulderBrightnessPercent > 100 ||
        settings.aboveHoldBrightnessPercent > 100)
    {
        error = "Brightness must be between 0 and 100 percent";
        return false;
    }

    for (size_t index = 0; index < settings.logicalMappingCount; ++index)
    {
        const uint16_t physicalLed = settings.logicalMapping[index];
        if (physicalLed >= settings.physicalLedCount)
        {
            error = "MoonBoard mapping contains an out-of-range LED ID";
            return false;
        }
        for (size_t previous = 0; previous < index; ++previous)
        {
            if (settings.logicalMapping[previous] == physicalLed)
            {
                error = "MoonBoard mapping contains a duplicate LED ID";
                return false;
            }
        }
    }

    if (settings.kickerLedCount > MAX_KICKER_LED_COUNT)
    {
        error = "Too many kicker LED IDs";
        return false;
    }
    for (size_t index = 0; index < settings.kickerLedCount; ++index)
    {
        const uint16_t physicalLed = settings.kickerLedIds[index];
        if (physicalLed >= settings.physicalLedCount)
        {
            error = "Kicker list contains an out-of-range LED ID";
            return false;
        }
        for (size_t previous = 0; previous < index; ++previous)
        {
            if (settings.kickerLedIds[previous] == physicalLed)
            {
                error = "Kicker list contains a duplicate LED ID";
                return false;
            }
        }
        for (size_t logical = 0; logical < settings.logicalMappingCount; ++logical)
        {
            if (settings.logicalMapping[logical] == physicalLed)
            {
                error = "Kicker list overlaps the MoonBoard mapping";
                return false;
            }
        }
    }

    error.clear();
    return true;
}

bool parseLedIdList(
    const std::string &text,
    uint16_t *output,
    size_t outputCapacity,
    uint16_t &outputCount,
    std::string &error)
{
    outputCount = 0;
    size_t position = 0;

    while (position < text.size())
    {
        while (
            position < text.size() &&
            std::isspace(static_cast<unsigned char>(text[position])))
        {
            ++position;
        }
        if (position == text.size())
            break;
        if (output == nullptr || outputCount >= outputCapacity)
        {
            error = "LED list contains too many entries";
            return false;
        }

        const char *start = text.c_str() + position;
        char *end = nullptr;
        const unsigned long value = std::strtoul(start, &end, 10);
        if (end == start || value > 65535)
        {
            error = "LED list contains an invalid ID";
            return false;
        }
        output[outputCount++] = static_cast<uint16_t>(value);
        position = static_cast<size_t>(end - text.c_str());

        while (
            position < text.size() &&
            std::isspace(static_cast<unsigned char>(text[position])))
        {
            ++position;
        }
        if (position == text.size())
            break;
        if (text[position] != ',')
        {
            error = "LED IDs must be separated by commas";
            return false;
        }
        ++position;
        while (
            position < text.size() &&
            std::isspace(static_cast<unsigned char>(text[position])))
        {
            ++position;
        }
        if (position == text.size())
        {
            error = "LED list must not end with a comma";
            return false;
        }
    }

    error.clear();
    return true;
}
