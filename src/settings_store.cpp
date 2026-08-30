#include "settings_store.h"

#include <Preferences.h>

#include <cstring>

namespace
{
constexpr char NAMESPACE_NAME[] = "moonboard";
constexpr uint16_t SETTINGS_SCHEMA_VERSION = 1;

uint32_t encodeColor(const RgbColor &color)
{
    return
        (static_cast<uint32_t>(color.red) << 16) |
        (static_cast<uint32_t>(color.green) << 8) |
        color.blue;
}

RgbColor decodeColor(uint32_t value)
{
    return RgbColor(
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF));
}
} // namespace

bool SettingsStore::load(
    RuntimeSettings &settings,
    std::string &error) const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE_NAME, true))
    {
        error = "Could not open NVS settings";
        return false;
    }

    if (
        preferences.getUShort("schema", 0) != SETTINGS_SCHEMA_VERSION ||
        preferences.getUShort("logical", 0) != settings.logicalMappingCount)
    {
        preferences.end();
        error = "No compatible saved settings; using firmware defaults";
        return false;
    }

    RuntimeSettings candidate = settings;
    preferences.getString(
        "host",
        candidate.wledHost,
        sizeof(candidate.wledHost));
    candidate.physicalLedCount = preferences.getUShort(
        "physical",
        candidate.physicalLedCount);
    candidate.segmentId = preferences.getUChar("segment", candidate.segmentId);
    candidate.boulderBrightnessPercent = preferences.getUChar(
        "boulder",
        candidate.boulderBrightnessPercent);
    candidate.aboveHoldBrightnessPercent = preferences.getUChar(
        "above",
        candidate.aboveHoldBrightnessPercent);
    candidate.routeTimeoutMinutes = preferences.getUShort(
        "timeout",
        candidate.routeTimeoutMinutes);
    candidate.kickerLedsEnabled = preferences.getBool(
        "kick_en",
        candidate.kickerLedsEnabled);
    candidate.kickerLedColor = decodeColor(preferences.getUInt(
        "kick_color",
        encodeColor(candidate.kickerLedColor)));
    candidate.kickerLedCount = preferences.getUShort(
        "kick_count",
        candidate.kickerLedCount);

    const size_t mappingBytes =
        candidate.logicalMappingCount * sizeof(candidate.logicalMapping[0]);
    const size_t kickerBytes =
        candidate.kickerLedCount * sizeof(candidate.kickerLedIds[0]);
    const bool blobsValid =
        preferences.getBytesLength("mapping") == mappingBytes &&
        candidate.kickerLedCount <= MAX_KICKER_LED_COUNT &&
        (kickerBytes == 0 ||
         preferences.getBytesLength("kick_ids") == kickerBytes);
    if (blobsValid)
    {
        preferences.getBytes(
            "mapping",
            candidate.logicalMapping,
            mappingBytes);
        if (kickerBytes > 0)
        {
            preferences.getBytes(
                "kick_ids",
                candidate.kickerLedIds,
                kickerBytes);
        }
    }
    preferences.end();

    if (!blobsValid)
    {
        error = "Saved LED lists are incomplete; using firmware defaults";
        return false;
    }
    if (!validateRuntimeSettings(candidate, error))
    {
        error = "Saved settings are invalid: " + error;
        return false;
    }

    settings = candidate;
    error.clear();
    return true;
}

bool SettingsStore::save(
    const RuntimeSettings &settings,
    std::string &error) const
{
    if (!validateRuntimeSettings(settings, error))
        return false;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE_NAME, false))
    {
        error = "Could not open NVS settings for writing";
        return false;
    }

    // Invalidate the stored record first and mark it compatible only after
    // every field has been written. A power loss during saving therefore
    // falls back to firmware defaults instead of loading a partial record.
    bool success = preferences.putUShort("schema", 0) > 0;
    success &= preferences.putUShort("logical", settings.logicalMappingCount) > 0;
    success &= preferences.putString("host", settings.wledHost) > 0;
    success &= preferences.putUShort("physical", settings.physicalLedCount) > 0;
    success &= preferences.putUChar("segment", settings.segmentId) > 0;
    success &= preferences.putUChar(
        "boulder",
        settings.boulderBrightnessPercent) > 0;
    success &= preferences.putUChar(
        "above",
        settings.aboveHoldBrightnessPercent) > 0;
    success &= preferences.putUShort(
        "timeout",
        settings.routeTimeoutMinutes) > 0;
    success &= preferences.putBool("kick_en", settings.kickerLedsEnabled) > 0;
    success &= preferences.putUInt(
        "kick_color",
        encodeColor(settings.kickerLedColor)) > 0;
    success &= preferences.putUShort("kick_count", settings.kickerLedCount) > 0;

    const size_t mappingBytes =
        settings.logicalMappingCount * sizeof(settings.logicalMapping[0]);
    const size_t kickerBytes =
        settings.kickerLedCount * sizeof(settings.kickerLedIds[0]);
    success &= preferences.putBytes(
        "mapping",
        settings.logicalMapping,
        mappingBytes) == mappingBytes;
    if (kickerBytes > 0)
    {
        success &= preferences.putBytes(
            "kick_ids",
            settings.kickerLedIds,
            kickerBytes) == kickerBytes;
    }
    else
    {
        if (preferences.getBytesLength("kick_ids") > 0)
            preferences.remove("kick_ids");
    }
    if (success)
    {
        success =
            preferences.putUShort("schema", SETTINGS_SCHEMA_VERSION) > 0;
    }
    preferences.end();

    if (!success)
    {
        error = "One or more NVS values could not be written";
        return false;
    }
    error.clear();
    return true;
}
