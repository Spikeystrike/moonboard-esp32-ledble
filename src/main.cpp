#include <Arduino.h>
#include <ETH.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "app_log.h"
#include "config.h"
#include "led_mapping.h"
#include "moonboard_ble.h"
#include "moonboard_protocol.h"
#include "route_timeout.h"
#include "runtime_settings.h"
#include "settings_store.h"
#include "settings_web.h"
#include "wled_client.h"

MoonboardBleServer bleSerial;
MoonboardProtocolParser protocolParser;
RuntimeSettings runtimeSettings = {};
WledControllerConfig runtimeWledController = {
    runtimeSettings.wledHost,
    0,
    0,
    0,
};
WledClient wledClient(
    &runtimeWledController,
    1,
    WLED_HTTP_TIMEOUT_MS,
    WLED_WAKE_DELAY_MS,
    WLED_RETRY_DELAY_MS);
SettingsStore settingsStore;
SettingsWebServer settingsWebServer;

RgbColor leds[MAX_PHYSICAL_LED_COUNT];
bool ledAboveHoldEnabled = false;
bool lastEthernetReady = false;
bool routeAlwaysOnLedConfigurationValid = false;
bool routeActive = false;
uint32_t routeStartedAtMs = 0;
bool calibrationLedActive = false;
uint32_t calibrationLedStartedAtMs = 0;
unsigned long lastEthernetStatusLog = 0;
bool lastBleConnected = false;
bool bleConnectionStateInitialized = false;

namespace
{
bool ethernetReady()
{
    return ETH.linkUp() && ETH.localIP() != IPAddress(0, 0, 0, 0);
}

void clearLocalLeds()
{
    std::fill(leds, leds + MAX_PHYSICAL_LED_COUNT, RgbColor());
}

void loadFirmwareDefaults()
{
    std::memset(&runtimeSettings, 0, sizeof(runtimeSettings));
    std::strncpy(
        runtimeSettings.wledHost,
        DEFAULT_WLED_HOST,
        sizeof(runtimeSettings.wledHost));
    runtimeSettings.wledHost[sizeof(runtimeSettings.wledHost) - 1] = '\0';
    runtimeSettings.physicalLedCount = PHYSICAL_LED_COUNT;
    runtimeSettings.segmentId = DEFAULT_WLED_SEGMENT_ID;
    runtimeSettings.boulderBrightnessPercent = BOULDER_BRIGHTNESS_PERCENT;
    runtimeSettings.aboveHoldBrightnessPercent =
        ABOVE_HOLD_BRIGHTNESS_PERCENT;
    runtimeSettings.routeTimeoutMinutes = ROUTE_TIMEOUT_MINUTES;
    runtimeSettings.kickerLedsEnabled = ROUTE_ALWAYS_ON_LEDS_ENABLED;
    runtimeSettings.kickerLedColor = ROUTE_ALWAYS_ON_LED_COLOR;
    runtimeSettings.logicalMappingCount = LOGICAL_LED_COUNT;
    std::copy(
        LOGICAL_TO_PHYSICAL_LED,
        LOGICAL_TO_PHYSICAL_LED + LOGICAL_LED_COUNT,
        runtimeSettings.logicalMapping);
    runtimeSettings.kickerLedCount = ROUTE_ALWAYS_ON_LED_COUNT;
    std::copy(
        ROUTE_ALWAYS_ON_LED_IDS,
        ROUTE_ALWAYS_ON_LED_IDS + ROUTE_ALWAYS_ON_LED_COUNT,
        runtimeSettings.kickerLedIds);
}

void applyRuntimeSettings()
{
    runtimeWledController.host = runtimeSettings.wledHost;
    runtimeWledController.firstGlobalLed = 0;
    runtimeWledController.lastGlobalLed = static_cast<uint16_t>(
        runtimeSettings.physicalLedCount - 1);
    runtimeWledController.segmentId = runtimeSettings.segmentId;

    routeAlwaysOnLedConfigurationValid = validateUnmappedPhysicalLeds(
        runtimeSettings.kickerLedIds,
        runtimeSettings.kickerLedCount,
        runtimeSettings.physicalLedCount,
        runtimeSettings.logicalMapping,
        runtimeSettings.logicalMappingCount);
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
            runtimeSettings.logicalMapping,
            runtimeSettings.logicalMappingCount,
            runtimeSettings.physicalLedCount,
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
               runtimeSettings.logicalMapping,
               runtimeSettings.logicalMappingCount,
               runtimeSettings.physicalLedCount,
               physicalPosition) ||
           leds[physicalPosition].isBlack();
}

void applyRouteAlwaysOnLeds()
{
    if (
        !runtimeSettings.kickerLedsEnabled ||
        !routeAlwaysOnLedConfigurationValid)
    {
        return;
    }

    for (size_t index = 0; index < runtimeSettings.kickerLedCount; ++index)
    {
        leds[runtimeSettings.kickerLedIds[index]] =
            runtimeSettings.kickerLedColor;
    }
}

void resetLights()
{
    routeActive = false;
    calibrationLedActive = false;
    clearLocalLeds();
    if (ethernetReady())
    {
        if (!wledClient.reset())
            appLogLine("[WLED] Could not queue the reset frame");
    }
    else
        appLogLine("[WLED] Reset deferred: Ethernet is not ready");
}

void maintainRouteTimeout()
{
    if (!routeTimeoutExpired(
            routeActive,
            runtimeSettings.routeTimeoutMinutes,
            routeStartedAtMs,
            static_cast<uint32_t>(millis())))
    {
        return;
    }

    appLogPrintf(
        "[ROUTE] Timeout after %u minutes; switching all LEDs off\n",
        runtimeSettings.routeTimeoutMinutes);
    resetLights();
}

void maintainCalibrationLedTimeout()
{
    if (
        !calibrationLedActive ||
        static_cast<uint32_t>(millis()) - calibrationLedStartedAtMs <
            CALIBRATION_LED_TIMEOUT_MS)
    {
        return;
    }

    appLogLine("[CALIBRATION] Test LED timeout; switching all LEDs off");
    resetLights();
}

bool saveAndApplyRuntimeSettings(
    const RuntimeSettings &candidate,
    std::string &error)
{
    if (!validateRuntimeSettings(candidate, error))
        return false;
    if (!settingsStore.save(candidate, error))
        return false;

    if (ethernetReady() && !wledClient.reset())
        appLogLine("[WLED] Could not queue the old settings reset");
    runtimeSettings = candidate;
    applyRuntimeSettings();
    if (ethernetReady() && !wledClient.reset())
        appLogLine("[WLED] Could not queue the new settings reset");
    routeActive = false;
    calibrationLedActive = false;
    clearLocalLeds();
    appLogLine("[SETTINGS] Runtime settings saved to NVS");
    return true;
}

bool showCalibrationLed(uint16_t physicalLed, std::string &error)
{
    if (physicalLed >= runtimeSettings.physicalLedCount)
    {
        error = "Physical LED ID is outside the configured WLED range";
        return false;
    }
    if (!ethernetReady())
    {
        error = "Ethernet is not ready";
        return false;
    }

    routeActive = false;
    clearLocalLeds();
    leds[physicalLed] = COLOR_WHITE;
    if (!wledClient.render(
            leds,
            runtimeSettings.physicalLedCount,
            CALIBRATION_BRIGHTNESS_PERCENT))
    {
        error = "The calibration frame could not be queued for WLED";
        return false;
    }
    calibrationLedActive = true;
    calibrationLedStartedAtMs = static_cast<uint32_t>(millis());
    appLogPrintf("[CALIBRATION] Testing physical WLED ID %u\n", physicalLed);
    error.clear();
    return true;
}

void processConfiguration(const std::string &message)
{
    appLogPrintf("[BLE] Configuration: %s\n", message.c_str());

    if (message.find("~D") != std::string::npos)
    {
        ledAboveHoldEnabled = true;
        appLogLine("[BLE] Additional light above each hold enabled");
    }
    else if (message.find("~M") != std::string::npos)
    {
        ledAboveHoldEnabled = false;
    }

    if (message.find("~Z*") != std::string::npos)
    {
        appLogLine("[BLE] Reset lights");
        resetLights();
    }
}

void processProblem(const std::string &message)
{
    appLogPrintf("[BLE] Problem: %s\n", message.c_str());
    calibrationLedActive = false;
    clearLocalLeds();

    std::vector<MoonboardHold> holds;
    if (!parseProblem(message, holds))
    {
        appLogLine("[BLE] Ignoring malformed problem message");
        routeActive = false;
        if (ethernetReady() && !wledClient.reset())
            appLogLine("[WLED] Could not queue the malformed-route reset");
        ledAboveHoldEnabled = false;
        return;
    }

    std::vector<MoonboardHold> validHolds;
    std::string coordinates;
    for (const MoonboardHold &hold : holds)
    {
        if (hold.position >= LOGICAL_LED_COUNT)
        {
            appLogPrintf(
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
            runtimeSettings.aboveHoldBrightnessPercent);
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

    appLogPrintf(
        "[WLED] Rendering %u holds (%s)\n",
        static_cast<unsigned>(validHolds.size()),
        coordinates.c_str());
    if (ethernetReady())
    {
        if (!wledClient.render(
                leds,
                runtimeSettings.physicalLedCount,
                runtimeSettings.boulderBrightnessPercent))
        {
            appLogLine("[WLED] Could not queue the route frame");
        }
    }
    else
    {
        appLogLine("[WLED] Render deferred: Ethernet is not ready");
    }
    ledAboveHoldEnabled = false;
}

void maintainBle()
{
    const bool connected = bleSerial.connected();
    const bool connectionChanged =
        !bleConnectionStateInitialized || connected != lastBleConnected;

    if (connectionChanged && connected)
    {
        // Keep already-buffered bytes: the app may write the first route
        // immediately after connecting, before loop() observes the link.
        protocolParser.reset();
        appLogLine("[BLE] MoonBoard app connected");
    }
    else if (
        bleConnectionStateInitialized &&
        connectionChanged &&
        !connected)
    {
        appLogLine("[BLE] MoonBoard app disconnected");
    }

    MoonboardBleReceiveStats receiveStats;
    if (bleSerial.takeReceiveStats(receiveStats))
    {
        appLogPrintf(
            "[BLE RX] %u write(s), %u byte(s); last=%u, max=%u\n",
            static_cast<unsigned>(receiveStats.writeCount),
            static_cast<unsigned>(receiveStats.byteCount),
            static_cast<unsigned>(receiveStats.lastWriteLength),
            static_cast<unsigned>(receiveStats.maximumWriteLength));
    }

    if (bleSerial.consumeOverflow())
    {
        protocolParser.reset();
        appLogLine(
            "[BLE] Receive buffer overflow; waiting for the next route");
    }

    // Drain received data even during a connection-state transition. The old
    // BLESerial implementation gated reads on connected(), which could leave
    // the first app write unprocessed while the ESP32 link state was settling.
    while (bleSerial.available() > 0)
    {
        const int received = bleSerial.read();
        if (received < 0)
            break;

        ProtocolMessage message;
        if (!protocolParser.feed(static_cast<char>(received), message))
            continue;

        if (message.type == ProtocolMessageType::Configuration)
            processConfiguration(message.payload);
        else
            processProblem(message.payload);
    }

    if (connectionChanged && !connected)
        protocolParser.reset();
    lastBleConnected = connected;
    bleConnectionStateInitialized = true;
}

bool initializeEthernet()
{
    appLogLine("[ETH] Starting Olimex ESP32-POE-ISO Ethernet");
    if (!ETH.begin())
    {
        appLogLine("[ETH] Initialization failed");
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
        appLogLine(
            "[ETH] No link or DHCP address yet; BLE remains available");
        return false;
    }

    appLogPrintf(
        "[ETH] Connected: %s\n",
        ETH.localIP().toString().c_str());
    return true;
}

void checkWledAtBoot()
{
    if (!WLED_CHECK_AT_BOOT || !ethernetReady())
        return;

    const RgbColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE};
    appLogLine("[WLED] Running network LED check");
    for (const RgbColor &color : colors)
    {
        clearLocalLeds();
        for (uint16_t position = 0; position < LOGICAL_LED_COUNT; ++position)
            setLogicalLed(position, color);
        if (!wledClient.render(
                leds,
                runtimeSettings.physicalLedCount,
                runtimeSettings.boulderBrightnessPercent))
        {
            appLogLine("[WLED] Could not queue the boot-check frame");
        }
        delay(WLED_CHECK_COLOR_DELAY_MS);
    }
    resetLights();
}

void maintainEthernet()
{
    const bool ready = ethernetReady();
    if (ready && !lastEthernetReady)
    {
        appLogPrintf(
            "[ETH] Connected: %s\n",
            ETH.localIP().toString().c_str());
        appLogPrintf(
            "[WEB] Open http://%s/ for settings and calibration\n",
            ETH.localIP().toString().c_str());
        appLogPrintf(
            "[WEB] Open http://%s/logs for the remote live log\n",
            ETH.localIP().toString().c_str());
        appLogPrintf(
            "[OTA] Open http://%s/ota for firmware updates\n",
            ETH.localIP().toString().c_str());
        if (!wledClient.render(
                leds,
                runtimeSettings.physicalLedCount,
                calibrationLedActive
                    ? CALIBRATION_BRIGHTNESS_PERCENT
                    : runtimeSettings.boulderBrightnessPercent))
        {
            appLogLine("[WLED] Could not queue the reconnect frame");
        }
    }
    else if (!ready && lastEthernetReady)
    {
        appLogLine("[ETH] Connection lost");
    }
    else if (
        !ready &&
        millis() - lastEthernetStatusLog >= ETHERNET_STATUS_LOG_INTERVAL_MS)
    {
        appLogLine("[ETH] Waiting for link and DHCP");
        lastEthernetStatusLog = millis();
    }
    lastEthernetReady = ready;
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(200);
    appLogPrintf(
        "[SETUP] MoonBoard %s on Olimex ESP32-POE-ISO\n",
        BOARD_NAME);

    loadFirmwareDefaults();
    std::string settingsMessage;
    if (settingsStore.load(runtimeSettings, settingsMessage))
        appLogLine("[SETTINGS] Loaded runtime settings from NVS");
    else
        appLogPrintf("[SETTINGS] %s\n", settingsMessage.c_str());
    applyRuntimeSettings();

    if (!wledClient.begin())
        appLogLine("[SETUP] WLED background output is unavailable");
    if (!wledClient.validateConfiguration())
        appLogLine("[SETUP] WLED configuration contains errors");

    if (!validateLedMapping(
            runtimeSettings.logicalMapping,
            runtimeSettings.logicalMappingCount,
            runtimeSettings.physicalLedCount))
    {
        appLogLine(
            "[SETUP] LED mapping contains duplicates or invalid WLED IDs");
    }

    if (
        runtimeSettings.kickerLedsEnabled &&
        !routeAlwaysOnLedConfigurationValid)
    {
        appLogLine(
            "[SETUP] Always-on LED list contains duplicates, mapped LEDs, "
            "or invalid WLED IDs");
    }

    appLogLine("[BLE] Starting Nordic UART service as MoonBoard");
    if (!bleSerial.begin(BLE_NAME))
        appLogLine("[BLE] Initialization failed");

    lastEthernetReady = initializeEthernet();
    settingsWebServer.begin(
        &runtimeSettings,
        &appLogBuffer(),
        BOARD_NAME,
        BOARD_ROWS,
        saveAndApplyRuntimeSettings,
        showCalibrationLed,
        resetLights);
    appLogLine(
        "[WEB] Settings, calibration, live log, and OTA listening on port 80");
    if (ethernetReady())
    {
        appLogPrintf(
            "[WEB] Open http://%s/ for settings and calibration\n",
            ETH.localIP().toString().c_str());
        appLogPrintf(
            "[WEB] Open http://%s/logs for the remote live log\n",
            ETH.localIP().toString().c_str());
        appLogPrintf(
            "[OTA] Open http://%s/ota for firmware updates\n",
            ETH.localIP().toString().c_str());
    }
    checkWledAtBoot();
    clearLocalLeds();
    appLogLine("[SETUP] Waiting for the MoonBoard app");
}

void loop()
{
    settingsWebServer.handleClient();
    maintainCalibrationLedTimeout();
    maintainRouteTimeout();
    maintainEthernet();

    maintainBle();

    delay(1);
}
