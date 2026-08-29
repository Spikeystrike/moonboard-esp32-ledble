#include <unity.h>

#include <string>
#include <vector>

#include "config.h"
#include "led_mapping.h"
#include "moonboard_protocol.h"
#include "route_timeout.h"
#include "wled_payload.h"

void setUp() {}
void tearDown() {}

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

void test_wled_payload_is_empty_without_lit_pixels()
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
    TEST_ASSERT_TRUE(payload.empty());
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

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_parser_emits_configuration_and_problem);
    RUN_TEST(test_problem_parser_accepts_known_hold_types);
    RUN_TEST(test_problem_parser_rejects_malformed_tokens);
    RUN_TEST(test_snake_coordinates_and_above_hold_mapping);
    RUN_TEST(test_wled_payload_uses_local_ids_and_scaled_hex_colors);
    RUN_TEST(test_wled_payload_is_empty_without_lit_pixels);
    RUN_TEST(test_explicit_led_mapping_supports_leading_leds_and_gaps);
    RUN_TEST(test_led_mapping_validation_rejects_duplicates_and_invalid_ids);
    RUN_TEST(test_checked_in_example_mapping_is_valid);
    RUN_TEST(test_unmapped_physical_led_validation);
    RUN_TEST(test_checked_in_always_on_led_list_is_valid);
    RUN_TEST(test_route_timeout_can_be_disabled_and_restarts_per_route);
    RUN_TEST(test_route_timeout_handles_millis_wraparound);
    return UNITY_END();
}
