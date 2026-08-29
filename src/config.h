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
#else
const uint16_t LOGICAL_LED_COUNT = 200;
const uint8_t BOARD_ROWS = 18;
const char BOARD_NAME[] = "Standard";
#endif

// Each MoonBoard position maps to every Nth physical LED. For example, an
// offset of 2 maps positions 0, 1, 2 to physical WLED IDs 0, 2, 4.
const uint8_t LED_OFFSET = 2;
const uint16_t PHYSICAL_LED_COUNT = LOGICAL_LED_COUNT * LED_OFFSET;

const uint8_t BOULDER_BRIGHTNESS_PERCENT = 80;
const uint8_t ABOVE_HOLD_BRIGHTNESS_PERCENT = 10;
const bool WLED_CHECK_AT_BOOT = true;
const unsigned long WLED_CHECK_COLOR_DELAY_MS = 500;

const unsigned long ETHERNET_CONNECT_TIMEOUT_MS = 15000;
const unsigned long ETHERNET_STATUS_LOG_INTERVAL_MS = 5000;
const uint16_t WLED_HTTP_TIMEOUT_MS = 2000;

const char BLE_NAME[] = "MoonBoard";

// WLED ranges use inclusive global physical LED IDs, matching the convention
// used by cruxwledbridge. Configure one or more non-overlapping controllers.
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
