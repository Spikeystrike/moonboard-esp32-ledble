#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "led_mapping.h"
#include "live_log.h"
#include "moonboard_protocol.h"
#include "route_timeout.h"
#include "runtime_settings.h"
#include "wled_payload.h"

void setUp() {}
void tearDown() {}

RuntimeSettings validRuntimeSettings()
{
    RuntimeSettings settings = {};
    std::strcpy(settings.wledHost, "192.0.2.10");
    settings.physicalLedCount = 20;
    settings.segmentId = 0;
    settings.boulderBrightnessPercent = 80;
    settings.aboveHoldBrightnessPercent = 10;
    settings.routeTimeoutMinutes = 15;
    settings.kickerLedsEnabled = true;
    settings.kickerLedColor = RgbColor(255, 255, 255);
    settings.logicalMappingCount = 4;
    settings.logicalMapping[0] = 10;
    settings.logicalMapping[1] = 11;
    settings.logicalMapping[2] = 15;
    settings.logicalMapping[3] = 14;
    settings.kickerLedCount = 3;
    settings.kickerLedIds[0] = 0;
    settings.kickerLedIds[1] = 3;
    settings.kickerLedIds[2] = 9;
    return settings;
}

void test_parser_emits_configuration_and_problem()
{
    MoonboardProtocolParser parser;
    std::vector<ProtocolMessage> messages;
    const std::string input = "~D,M*l#S69,P82,E54#";

    for (char value : input)
    {
        ProtocolMessage message;
        if (parser.feed(value, message))
            messages.push_back(message);
    }

    TEST_ASSERT_EQUAL_UINT32(2, messages.size());
    TEST_ASSERT_TRUE(
        messages[0].type == ProtocolMessageType::Configuration);
    TEST_ASSERT_EQUAL_STRING("~D,M*", messages[0].payload.c_str());
    TEST_ASSERT_TRUE(messages[1].type == ProtocolMessageType::Problem);
    TEST_ASSERT_EQUAL_STRING("S69,P82,E54", messages[1].payload.c_str());
}

void test_parser_recovers_from_truncated_route_at_next_route_marker()
{
    MoonboardProtocolParser parser;
    ProtocolMessage message;
    const std::string truncated = "l#S1,P2";
    for (char value : truncated)
        TEST_ASSERT_FALSE(parser.feed(value, message));

    std::vector<ProtocolMessage> messages;
    const std::string complete = "l#S3,P4,E5#";
    for (char value : complete)
    {
        if (parser.feed(value, message))
            messages.push_back(message);
    }

    TEST_ASSERT_EQUAL_UINT32(1, messages.size());
    TEST_ASSERT_TRUE(messages[0].type == ProtocolMessageType::Problem);
    TEST_ASSERT_EQUAL_STRING("S3,P4,E5", messages[0].payload.c_str());
}

void test_parser_reset_discards_partial_ble_frame()
{
    MoonboardProtocolParser parser;
    ProtocolMessage message;
    for (char value : std::string("l#S1,P2"))
        TEST_ASSERT_FALSE(parser.feed(value, message));

    parser.reset();
    std::vector<ProtocolMessage> messages;
    for (char value : std::string("l#S7,E8#"))
    {
        if (parser.feed(value, message))
            messages.push_back(message);
    }

    TEST_ASSERT_EQUAL_UINT32(1, messages.size());
    TEST_ASSERT_EQUAL_STRING("S7,E8", messages[0].payload.c_str());
}

void test_problem_parser_accepts_known_hold_types()
{
    std::vector<MoonboardHold> holds;

    TEST_ASSERT_TRUE(parseProblem("S0,P12,F23,L24,R25,M26,E149", holds));
    TEST_ASSERT_EQUAL_UINT32(7, holds.size());
    TEST_ASSERT_EQUAL_CHAR('S', holds.front().type);
    TEST_ASSERT_EQUAL_UINT16(0, holds.front().position);
    TEST_ASSERT_EQUAL_CHAR('E', holds.back().type);
    TEST_ASSERT_EQUAL_UINT16(149, holds.back().position);
}

void test_problem_parser_rejects_malformed_tokens()
{
    std::vector<MoonboardHold> holds;

    TEST_ASSERT_FALSE(parseProblem("", holds));
    TEST_ASSERT_FALSE(parseProblem("X12", holds));
    TEST_ASSERT_FALSE(parseProblem("S12,P", holds));
    TEST_ASSERT_FALSE(parseProblem("S12,", holds));
    TEST_ASSERT_FALSE(parseProblem("S-1", holds));
}

void test_snake_coordinates_and_above_hold_mapping()
{
    TEST_ASSERT_EQUAL_STRING("A1", positionToCoordinates(0, 18).c_str());
    TEST_ASSERT_EQUAL_STRING("A18", positionToCoordinates(17, 18).c_str());
    TEST_ASSERT_EQUAL_STRING("B18", positionToCoordinates(18, 18).c_str());
    TEST_ASSERT_EQUAL_STRING("B1", positionToCoordinates(35, 18).c_str());
    TEST_ASSERT_EQUAL_STRING("C1", positionToCoordinates(36, 18).c_str());

    TEST_ASSERT_EQUAL_INT(1, aboveHoldPosition(0, 18));
    TEST_ASSERT_EQUAL_INT(-1, aboveHoldPosition(17, 18));
    TEST_ASSERT_EQUAL_INT(-1, aboveHoldPosition(18, 18));
    TEST_ASSERT_EQUAL_INT(34, aboveHoldPosition(35, 18));
}

void test_wled_payload_uses_local_ids_and_scaled_hex_colors()
{
    RgbColor leds[103];
    leds[100] = RgbColor(255, 0, 0);
    leds[102] = RgbColor(0, 0, 255);
    const WledControllerConfig controller = {
        "192.0.2.10",
        100,
        102,
        0,
    };
    size_t litLedCount = 0;

    const std::string payload = buildWledStatePayload(
        leds,
        103,
        controller,
        50,
        litLedCount);

    TEST_ASSERT_EQUAL_UINT32(2, litLedCount);
    TEST_ASSERT_EQUAL_STRING(
        "{\"seg\":{\"id\":0,\"start\":0,\"stop\":3,\"fx\":0,"
        "\"i\":[0,3,\"000000\",0,\"800000\",2,\"000080\"]}}",
        payload.c_str());
}

void test_wled_payload_clears_complete_range_without_lit_pixels()
{
    RgbColor leds[3];
    const WledControllerConfig controller = {
        "192.0.2.10",
        0,
        2,
        0,
    };
    size_t litLedCount = 99;

    const std::string payload = buildWledStatePayload(
        leds,
        3,
        controller,
        100,
        litLedCount);

    TEST_ASSERT_EQUAL_UINT32(0, litLedCount);
    TEST_ASSERT_EQUAL_STRING(
        "{\"seg\":{\"id\":0,\"start\":0,\"stop\":3,\"fx\":0,"
        "\"i\":[0,3,\"000000\"]}}",
        payload.c_str());
}

void test_wled_clear_payload_uses_local_range_and_segment()
{
    const WledControllerConfig controller = {
        "192.0.2.10",
        100,
        102,
        4,
    };

    const std::string payload = buildWledClearPayload(controller);

    TEST_ASSERT_EQUAL_STRING(
        "{\"seg\":{\"id\":4,\"start\":0,\"stop\":3,\"fx\":0,"
        "\"i\":[0,3,\"000000\"]}}",
        payload.c_str());
}

void test_explicit_led_mapping_supports_leading_leds_and_gaps()
{
    const uint16_t mapping[] = {10, 11, 15, 14};
    uint16_t physicalPosition = 0;

    TEST_ASSERT_TRUE(logicalToPhysicalLed(0, mapping, 4, 20, physicalPosition));
    TEST_ASSERT_EQUAL_UINT16(10, physicalPosition);
    TEST_ASSERT_TRUE(logicalToPhysicalLed(2, mapping, 4, 20, physicalPosition));
    TEST_ASSERT_EQUAL_UINT16(15, physicalPosition);
    TEST_ASSERT_TRUE(logicalToPhysicalLed(3, mapping, 4, 20, physicalPosition));
    TEST_ASSERT_EQUAL_UINT16(14, physicalPosition);
    TEST_ASSERT_FALSE(logicalToPhysicalLed(4, mapping, 4, 20, physicalPosition));
}

void test_led_mapping_validation_rejects_duplicates_and_invalid_ids()
{
    const uint16_t validMapping[] = {10, 11, 15, 14};
    const uint16_t duplicateMapping[] = {10, 11, 10};
    const uint16_t outOfRangeMapping[] = {10, 20};

    TEST_ASSERT_TRUE(validateLedMapping(validMapping, 4, 20));
    TEST_ASSERT_FALSE(validateLedMapping(duplicateMapping, 3, 20));
    TEST_ASSERT_FALSE(validateLedMapping(outOfRangeMapping, 2, 20));
}

void test_checked_in_example_mapping_is_valid()
{
    TEST_ASSERT_TRUE(validateLedMapping(
        LOGICAL_TO_PHYSICAL_LED,
        LED_MAPPING_COUNT,
        PHYSICAL_LED_COUNT));
    TEST_ASSERT_EQUAL_UINT16(10, LOGICAL_TO_PHYSICAL_LED[0]);
}

void test_unmapped_physical_led_validation()
{
    const uint16_t logicalMapping[] = {10, 11, 15, 14};
    const uint16_t validAlwaysOn[] = {0, 3, 9};
    const uint16_t duplicateAlwaysOn[] = {0, 3, 3};
    const uint16_t overlappingAlwaysOn[] = {0, 15};
    const uint16_t outOfRangeAlwaysOn[] = {0, 20};

    TEST_ASSERT_TRUE(validateUnmappedPhysicalLeds(
        validAlwaysOn, 3, 20, logicalMapping, 4));
    TEST_ASSERT_TRUE(validateUnmappedPhysicalLeds(
        nullptr, 0, 20, logicalMapping, 4));
    TEST_ASSERT_FALSE(validateUnmappedPhysicalLeds(
        duplicateAlwaysOn, 3, 20, logicalMapping, 4));
    TEST_ASSERT_FALSE(validateUnmappedPhysicalLeds(
        overlappingAlwaysOn, 2, 20, logicalMapping, 4));
    TEST_ASSERT_FALSE(validateUnmappedPhysicalLeds(
        outOfRangeAlwaysOn, 2, 20, logicalMapping, 4));
}

void test_checked_in_always_on_led_list_is_valid()
{
    TEST_ASSERT_TRUE(validateUnmappedPhysicalLeds(
        ROUTE_ALWAYS_ON_LED_IDS,
        ROUTE_ALWAYS_ON_LED_COUNT,
        PHYSICAL_LED_COUNT,
        LOGICAL_TO_PHYSICAL_LED,
        LED_MAPPING_COUNT));
}

void test_route_timeout_can_be_disabled_and_restarts_per_route()
{
    TEST_ASSERT_FALSE(routeTimeoutExpired(false, 15, 1000, 901000));
    TEST_ASSERT_FALSE(routeTimeoutExpired(true, 0, 1000, 901000));
    TEST_ASSERT_FALSE(routeTimeoutExpired(true, 15, 1000, 900999));
    TEST_ASSERT_TRUE(routeTimeoutExpired(true, 15, 1000, 901000));

    // A newly selected route gets a new start time.
    TEST_ASSERT_FALSE(routeTimeoutExpired(true, 15, 500000, 901000));
}

void test_route_timeout_handles_millis_wraparound()
{
    const uint32_t startedAt = 0xFFFFFFFFu - 30000u;
    TEST_ASSERT_FALSE(routeTimeoutExpired(true, 1, startedAt, 29998u));
    TEST_ASSERT_TRUE(routeTimeoutExpired(true, 1, startedAt, 30000u));
}

void test_runtime_settings_validation_accepts_valid_configuration()
{
    const RuntimeSettings settings = validRuntimeSettings();
    std::string error;

    TEST_ASSERT_TRUE(validateRuntimeSettings(settings, error));
    TEST_ASSERT_TRUE(error.empty());
}

void test_runtime_settings_validation_rejects_invalid_led_assignments()
{
    std::string error;
    RuntimeSettings settings = validRuntimeSettings();
    settings.logicalMapping[2] = settings.logicalMapping[0];
    TEST_ASSERT_FALSE(validateRuntimeSettings(settings, error));

    settings = validRuntimeSettings();
    settings.kickerLedIds[1] = settings.logicalMapping[2];
    TEST_ASSERT_FALSE(validateRuntimeSettings(settings, error));

    settings = validRuntimeSettings();
    settings.logicalMapping[3] = settings.physicalLedCount;
    TEST_ASSERT_FALSE(validateRuntimeSettings(settings, error));
}

void test_led_id_list_parser_handles_csv_and_rejects_bad_input()
{
    uint16_t values[4] = {};
    uint16_t count = 0;
    std::string error;

    TEST_ASSERT_TRUE(parseLedIdList("10, 11,\n15", values, 4, count, error));
    TEST_ASSERT_EQUAL_UINT16(3, count);
    TEST_ASSERT_EQUAL_UINT16(10, values[0]);
    TEST_ASSERT_EQUAL_UINT16(11, values[1]);
    TEST_ASSERT_EQUAL_UINT16(15, values[2]);

    TEST_ASSERT_TRUE(parseLedIdList("", values, 4, count, error));
    TEST_ASSERT_EQUAL_UINT16(0, count);
    TEST_ASSERT_FALSE(parseLedIdList("10,   ", values, 4, count, error));
    TEST_ASSERT_FALSE(parseLedIdList("10;11", values, 4, count, error));
    TEST_ASSERT_FALSE(parseLedIdList("1,2,3,4,5", values, 4, count, error));
}

void test_live_log_returns_only_new_entries_and_escapes_json()
{
    LiveLogBuffer log;
    log.append(100, "first \"message\"");
    log.append(250, "second message");

    TEST_ASSERT_EQUAL_UINT32(2, log.size());
    TEST_ASSERT_EQUAL_UINT32(2, log.latestSequence());

    const std::string all = log.jsonSince(0);
    TEST_ASSERT_TRUE(all.find("first \\\"message\\\"") != std::string::npos);
    TEST_ASSERT_TRUE(all.find("second message") != std::string::npos);

    const std::string newEntries = log.jsonSince(1);
    TEST_ASSERT_TRUE(newEntries.find("first") == std::string::npos);
    TEST_ASSERT_TRUE(newEntries.find("second message") != std::string::npos);
}

void test_live_log_is_bounded_and_recovers_after_device_restart()
{
    LiveLogBuffer log;
    log.append(1, "discard-me");
    for (size_t index = 0; index < LIVE_LOG_ENTRY_CAPACITY; ++index)
    {
        const std::string message = "retained-" + std::to_string(index);
        log.append(static_cast<uint32_t>(index + 2), message.c_str());
    }

    TEST_ASSERT_EQUAL_UINT32(LIVE_LOG_ENTRY_CAPACITY, log.size());
    const std::string entries = log.jsonSince(9999);
    TEST_ASSERT_TRUE(entries.find("discard-me") == std::string::npos);
    TEST_ASSERT_TRUE(entries.find("retained-0") != std::string::npos);
    TEST_ASSERT_TRUE(entries.find("retained-79") != std::string::npos);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_parser_emits_configuration_and_problem);
    RUN_TEST(test_parser_recovers_from_truncated_route_at_next_route_marker);
    RUN_TEST(test_parser_reset_discards_partial_ble_frame);
    RUN_TEST(test_problem_parser_accepts_known_hold_types);
    RUN_TEST(test_problem_parser_rejects_malformed_tokens);
    RUN_TEST(test_snake_coordinates_and_above_hold_mapping);
    RUN_TEST(test_wled_payload_uses_local_ids_and_scaled_hex_colors);
    RUN_TEST(test_wled_payload_clears_complete_range_without_lit_pixels);
    RUN_TEST(test_wled_clear_payload_uses_local_range_and_segment);
    RUN_TEST(test_explicit_led_mapping_supports_leading_leds_and_gaps);
    RUN_TEST(test_led_mapping_validation_rejects_duplicates_and_invalid_ids);
    RUN_TEST(test_checked_in_example_mapping_is_valid);
    RUN_TEST(test_unmapped_physical_led_validation);
    RUN_TEST(test_checked_in_always_on_led_list_is_valid);
    RUN_TEST(test_route_timeout_can_be_disabled_and_restarts_per_route);
    RUN_TEST(test_route_timeout_handles_millis_wraparound);
    RUN_TEST(test_runtime_settings_validation_accepts_valid_configuration);
    RUN_TEST(test_runtime_settings_validation_rejects_invalid_led_assignments);
    RUN_TEST(test_led_id_list_parser_handles_csv_and_rejects_bad_input);
    RUN_TEST(test_live_log_returns_only_new_entries_and_escapes_json);
    RUN_TEST(test_live_log_is_bounded_and_recovers_after_device_restart);
    return UNITY_END();
}
