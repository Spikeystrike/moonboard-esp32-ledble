#include <Arduino.h>
#include <BLESerial.h>
#include <ETH.h>

#include <algorithm>
#include <string>
#include <vector>

#include "config.h"
#include "led_mapping.h"
#include "moonboard_protocol.h"
#include "route_timeout.h"
#include "wled_client.h"

BLESerial bleSerial;
MoonboardProtocolParser protocolParser;
WledClient wledClient(
    WLED_CONTROLLERS,
    WLED_CONTROLLER_COUNT,
    WLED_HTTP_TIMEOUT_MS);

RgbColor leds[PHYSICAL_LED_COUNT];
bool ledAboveHoldEnabled = false;
bool lastEthernetReady = false;
bool routeAlwaysOnLedConfigurationValid = false;
bool routeActive = false;
uint32_t routeStartedAtMs = 0;
unsigned long lastEthernetStatusLog = 0;

namespace
{
bool ethernetReady()
{
    return ETH.linkUp() && ETH.localIP() != IPAddress(0, 0, 0, 0);
}

void clearLocalLeds()
{
    std::fill(leds, leds + PHYSICAL_LED_COUNT, RgbColor());
}

RgbColor scaleColor(const RgbColor &color, uint8_t percent)
{
    const uint16_t boundedPercent = std::min<uint16_t>(percent, 100);
    return RgbColor(
        (color.red * boundedPercent + 50) / 100,
        (color.green * boundedPercent + 50) / 100,
        (color.blue * boundedPercent + 50) / 100);
}

RgbColor colorForHold(char holdType)
{
    switch (holdType)
    {
    case 'E':
        return COLOR_RED;
    case 'F':
        return COLOR_CYAN;
    case 'L':
        return COLOR_PURPLE;
    case 'M':
        return COLOR_PINK;
    case 'P':
    case 'R':
        return COLOR_BLUE;
    case 'S':
        return COLOR_GREEN;
    default:
        return RgbColor();
    }
}

bool setLogicalLed(uint16_t logicalPosition, const RgbColor &color)
{
    uint16_t physicalPosition = 0;
    if (!logicalToPhysicalLed(
            logicalPosition,
            LOGICAL_TO_PHYSICAL_LED,
            LED_MAPPING_COUNT,
            PHYSICAL_LED_COUNT,
            physicalPosition))
        return false;

    leds[physicalPosition] = color;
    return true;
}

bool logicalLedIsBlack(uint16_t logicalPosition)
{
    uint16_t physicalPosition = 0;
    return !logicalToPhysicalLed(
               logicalPosition,
               LOGICAL_TO_PHYSICAL_LED,
               LED_MAPPING_COUNT,
               PHYSICAL_LED_COUNT,
               physicalPosition) ||
           leds[physicalPosition].isBlack();
}

void applyRouteAlwaysOnLeds()
{
    if (
        !ROUTE_ALWAYS_ON_LEDS_ENABLED ||
        !routeAlwaysOnLedConfigurationValid)
    {
        return;
    }

    for (size_t index = 0; index < ROUTE_ALWAYS_ON_LED_COUNT; ++index)
        leds[ROUTE_ALWAYS_ON_LED_IDS[index]] = ROUTE_ALWAYS_ON_LED_COLOR;
}

void resetLights()
{
    routeActive = false;
    clearLocalLeds();
    if (ethernetReady())
        wledClient.reset();
    else
        Serial.println("[WLED] Reset deferred: Ethernet is not ready");
}

void maintainRouteTimeout()
{
    if (!routeTimeoutExpired(
            routeActive,
            ROUTE_TIMEOUT_MINUTES,
            routeStartedAtMs,
            static_cast<uint32_t>(millis())))
    {
        return;
    }

    Serial.printf(
        "[ROUTE] Timeout after %u minutes; switching all LEDs off\n",
        ROUTE_TIMEOUT_MINUTES);
    resetLights();
}

void processConfiguration(const std::string &message)
{
    Serial.printf("[BLE] Configuration: %s\n", message.c_str());

    if (message.find("~D") != std::string::npos)
    {
        ledAboveHoldEnabled = true;
        Serial.println("[BLE] Additional light above each hold enabled");
    }
    else if (message.find("~M") != std::string::npos)
    {
        ledAboveHoldEnabled = false;
    }

    if (message.find("~Z*") != std::string::npos)
    {
        Serial.println("[BLE] Reset lights");
        resetLights();
    }
}

void processProblem(const std::string &message)
{
    Serial.printf("[BLE] Problem: %s\n", message.c_str());
    clearLocalLeds();

    std::vector<MoonboardHold> holds;
    if (!parseProblem(message, holds))
    {
        Serial.println("[BLE] Ignoring malformed problem message");
        routeActive = false;
        if (ethernetReady())
            wledClient.reset();
        ledAboveHoldEnabled = false;
        return;
    }

    std::vector<MoonboardHold> validHolds;
    std::string coordinates;
    for (const MoonboardHold &hold : holds)
    {
        if (hold.position >= LOGICAL_LED_COUNT)
        {
            Serial.printf(
                "[BLE] Ignoring out-of-range hold %c%u\n",
                hold.type,
                hold.position);
            continue;
        }

        setLogicalLed(hold.position, colorForHold(hold.type));
        validHolds.push_back(hold);
        if (!coordinates.empty())
            coordinates += ' ';
        coordinates += positionToCoordinates(hold.position, BOARD_ROWS);
    }

    if (ledAboveHoldEnabled)
    {
        const RgbColor aboveColor = scaleColor(
            COLOR_WHITE,
            ABOVE_HOLD_BRIGHTNESS_PERCENT);
        for (const MoonboardHold &hold : validHolds)
        {
            const int above = aboveHoldPosition(hold.position, BOARD_ROWS);
            if (
                above >= 0 &&
                above < LOGICAL_LED_COUNT &&
                logicalLedIsBlack(above))
            {
                setLogicalLed(above, aboveColor);
            }
        }
    }

    if (!validHolds.empty())
    {
        applyRouteAlwaysOnLeds();
        routeActive = true;
        routeStartedAtMs = static_cast<uint32_t>(millis());
    }
    else
    {
        routeActive = false;
    }

    Serial.printf(
        "[WLED] Rendering %u holds (%s)\n",
        static_cast<unsigned>(validHolds.size()),
        coordinates.c_str());
    if (ethernetReady())
    {
        wledClient.render(
            leds,
            PHYSICAL_LED_COUNT,
            BOULDER_BRIGHTNESS_PERCENT);
    }
    else
    {
        Serial.println("[WLED] Render deferred: Ethernet is not ready");
    }
    ledAboveHoldEnabled = false;
}

bool initializeEthernet()
{
    Serial.println("[ETH] Starting Olimex ESP32-POE-ISO Ethernet");
    if (!ETH.begin())
    {
        Serial.println("[ETH] Initialization failed");
        return false;
    }

    const unsigned long started = millis();
    while (
        !ethernetReady() &&
        millis() - started < ETHERNET_CONNECT_TIMEOUT_MS)
    {
        delay(100);
    }

    if (!ethernetReady())
    {
        Serial.println(
            "[ETH] No link or DHCP address yet; BLE remains available");
        return false;
    }

    Serial.printf(
        "[ETH] Connected: %s\n",
        ETH.localIP().toString().c_str());
    return true;
}

void checkWledAtBoot()
{
    if (!WLED_CHECK_AT_BOOT || !ethernetReady())
        return;

    const RgbColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE};
    Serial.println("[WLED] Running network LED check");
    for (const RgbColor &color : colors)
    {
        clearLocalLeds();
        for (uint16_t position = 0; position < LOGICAL_LED_COUNT; ++position)
            setLogicalLed(position, color);
        wledClient.render(
            leds,
            PHYSICAL_LED_COUNT,
            BOULDER_BRIGHTNESS_PERCENT);
        delay(WLED_CHECK_COLOR_DELAY_MS);
    }
    resetLights();
}

void maintainEthernet()
{
    const bool ready = ethernetReady();
    if (ready && !lastEthernetReady)
    {
        Serial.printf(
            "[ETH] Connected: %s\n",
            ETH.localIP().toString().c_str());
        wledClient.render(
            leds,
            PHYSICAL_LED_COUNT,
            BOULDER_BRIGHTNESS_PERCENT);
    }
    else if (!ready && lastEthernetReady)
    {
        Serial.println("[ETH] Connection lost");
    }
    else if (
        !ready &&
        millis() - lastEthernetStatusLog >= ETHERNET_STATUS_LOG_INTERVAL_MS)
    {
        Serial.println("[ETH] Waiting for link and DHCP");
        lastEthernetStatusLog = millis();
    }
    lastEthernetReady = ready;
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.printf(
        "[SETUP] MoonBoard %s on Olimex ESP32-POE-ISO\n",
        BOARD_NAME);

    if (!wledClient.validateConfiguration())
        Serial.println("[SETUP] WLED configuration contains errors");

    if (!validateLedMapping(
            LOGICAL_TO_PHYSICAL_LED,
            LED_MAPPING_COUNT,
            PHYSICAL_LED_COUNT))
    {
        Serial.println(
            "[SETUP] LED mapping contains duplicates or invalid WLED IDs");
    }

    routeAlwaysOnLedConfigurationValid = validateUnmappedPhysicalLeds(
        ROUTE_ALWAYS_ON_LED_IDS,
        ROUTE_ALWAYS_ON_LED_COUNT,
        PHYSICAL_LED_COUNT,
        LOGICAL_TO_PHYSICAL_LED,
        LED_MAPPING_COUNT);
    if (
        ROUTE_ALWAYS_ON_LEDS_ENABLED &&
        !routeAlwaysOnLedConfigurationValid)
    {
        Serial.println(
            "[SETUP] Always-on LED list contains duplicates, mapped LEDs, "
            "or invalid WLED IDs");
    }

    Serial.println("[BLE] Starting Nordic UART service as MoonBoard");
    if (!bleSerial.begin(const_cast<char *>(BLE_NAME)))
        Serial.println("[BLE] Initialization failed");

    lastEthernetReady = initializeEthernet();
    checkWledAtBoot();
    clearLocalLeds();
    Serial.println("[SETUP] Waiting for the MoonBoard app");
}

void loop()
{
    maintainRouteTimeout();
    maintainEthernet();

    if (bleSerial.connected())
    {
        while (bleSerial.available())
        {
            const char value = static_cast<char>(bleSerial.read());
            ProtocolMessage message;
            if (!protocolParser.feed(value, message))
                continue;

            if (message.type == ProtocolMessageType::Configuration)
                processConfiguration(message.payload);
            else
                processProblem(message.payload);
        }
    }

    delay(1);
}
