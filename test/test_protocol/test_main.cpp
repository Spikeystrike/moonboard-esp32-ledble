#include <unity.h>

#include <string>
#include <vector>

#include "moonboard_protocol.h"
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
        "{\"on\":true,\"bri\":255,\"seg\":{\"id\":0,\"fx\":0,"
        "\"i\":[0,\"800000\",2,\"000080\"]}}",
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

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_parser_emits_configuration_and_problem);
    RUN_TEST(test_problem_parser_accepts_known_hold_types);
    RUN_TEST(test_problem_parser_rejects_malformed_tokens);
    RUN_TEST(test_snake_coordinates_and_above_hold_mapping);
    RUN_TEST(test_wled_payload_uses_local_ids_and_scaled_hex_colors);
    RUN_TEST(test_wled_payload_is_empty_without_lit_pixels);
    return UNITY_END();
}
