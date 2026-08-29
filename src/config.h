#pragma once

#include <stddef.h>

#include "bridge_types.h"

// Select exactly one MoonBoard layout.
// #define MOONBOARD_STANDARD
#define MOONBOARD_MINI

#if defined(MOONBOARD_MINI) && defined(MOONBOARD_STANDARD)
#error "Select only one MoonBoard layout"
#elif !defined(MOONBOARD_MINI) && !defined(MOONBOARD_STANDARD)
#error "Select a MoonBoard layout"
#endif

#if defined(MOONBOARD_MINI)
const uint16_t LOGICAL_LED_COUNT = 150;
const uint8_t BOARD_ROWS = 12;
const char BOARD_NAME[] = "Mini";

// Example installation: WLED IDs 0-9 are unrelated LEDs before the
// MoonBoard. One unrelated LED is skipped after every block of 12 MoonBoard
// LEDs. Replace every value with the physical WLED ID wired to the
// corresponding logical MoonBoard position (0 = A1).
constexpr uint16_t LOGICAL_TO_PHYSICAL_LED[] = {
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
    36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
    75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
    88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
    101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
    114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125,
    127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138,
    140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151,
    153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    166, 167, 168, 169, 170, 171,
};

// Total number of LEDs configured in this single WLED instance. The example
// also leaves WLED IDs 172-179 available after the MoonBoard.
constexpr uint16_t PHYSICAL_LED_COUNT = 180;
#else
const uint16_t LOGICAL_LED_COUNT = 200;
const uint8_t BOARD_ROWS = 18;
const char BOARD_NAME[] = "Standard";

// The Standard-board example follows the same scheme: ten leading LEDs and
// one skipped LED after every block of 18 MoonBoard LEDs.
constexpr uint16_t LOGICAL_TO_PHYSICAL_LED[] = {
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 29, 30, 31, 32, 33, 34,
    35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84,
    86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97,
    98, 99, 100, 101, 102, 103, 105, 106, 107, 108, 109, 110,
    111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122,
    124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135,
    136, 137, 138, 139, 140, 141, 143, 144, 145, 146, 147, 148,
    149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
    162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173,
    174, 175, 176, 177, 178, 179, 181, 182, 183, 184, 185, 186,
    187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198,
    200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211,
    212, 213, 214, 215, 216, 217, 219, 220,
};

constexpr uint16_t PHYSICAL_LED_COUNT = 230;
#endif

constexpr size_t LED_MAPPING_COUNT =
    sizeof(LOGICAL_TO_PHYSICAL_LED) / sizeof(LOGICAL_TO_PHYSICAL_LED[0]);
static_assert(
    LED_MAPPING_COUNT == LOGICAL_LED_COUNT,
    "LED mapping must contain exactly one entry per logical MoonBoard LED");

// Optional kicker LEDs that illuminate together with every valid route.
// These physical WLED IDs do not belong to the MoonBoard hold LEDs and must
// not occur in LOGICAL_TO_PHYSICAL_LED. The checked-in example uses four of
// the unrelated LEDs before the MoonBoard. Set this to false to leave the
// kicker LEDs off without deleting the list.
const bool ROUTE_ALWAYS_ON_LEDS_ENABLED = true;
constexpr uint16_t ROUTE_ALWAYS_ON_LED_IDS[] = {0, 3, 6, 9};
constexpr size_t ROUTE_ALWAYS_ON_LED_COUNT =
    sizeof(ROUTE_ALWAYS_ON_LED_IDS) / sizeof(ROUTE_ALWAYS_ON_LED_IDS[0]);

const uint8_t BOULDER_BRIGHTNESS_PERCENT = 80;
const uint8_t ABOVE_HOLD_BRIGHTNESS_PERCENT = 10;
const bool WLED_CHECK_AT_BOOT = true;
const unsigned long WLED_CHECK_COLOR_DELAY_MS = 500;

const unsigned long ETHERNET_CONNECT_TIMEOUT_MS = 15000;
const unsigned long ETHERNET_STATUS_LOG_INTERVAL_MS = 5000;
const uint16_t WLED_HTTP_TIMEOUT_MS = 2000;

const char BLE_NAME[] = "MoonBoard";

// WLED ranges use inclusive global physical LED IDs, matching the convention
// used by cruxwledbridge. This example uses one controller for the complete
// physical LED chain.
// The example address below is reserved for documentation and must be changed.
const WledControllerConfig WLED_CONTROLLERS[] = {
    {"192.0.2.10", 0, static_cast<uint16_t>(PHYSICAL_LED_COUNT - 1), 0},
};
const size_t WLED_CONTROLLER_COUNT =
    sizeof(WLED_CONTROLLERS) / sizeof(WLED_CONTROLLERS[0]);

const RgbColor COLOR_RED(255, 0, 0);
const RgbColor COLOR_GREEN(0, 255, 0);
const RgbColor COLOR_BLUE(0, 0, 255);
const RgbColor COLOR_CYAN(0, 128, 128);
const RgbColor COLOR_PINK(120, 50, 85);
const RgbColor COLOR_PURPLE(105, 0, 150);
const RgbColor COLOR_WHITE(255, 255, 255);

// Always-on LEDs use the same BOULDER_BRIGHTNESS_PERCENT scaling as holds.
const RgbColor ROUTE_ALWAYS_ON_LED_COLOR(255, 255, 255);
