#pragma once

#include <WebServer.h>

#include <functional>
#include <string>

#include "runtime_settings.h"

class SettingsWebServer
{
public:
    using ApplySettingsHandler = std::function<bool(
        const RuntimeSettings &,
        std::string &)>;
    using TestLedHandler = std::function<bool(uint16_t, std::string &)>;
    using AllOffHandler = std::function<void()>;

    SettingsWebServer();

    void begin(
        RuntimeSettings *settings,
        const char *boardName,
        uint8_t boardRows,
        ApplySettingsHandler applySettings,
        TestLedHandler testLed,
        AllOffHandler allOff);
    void handleClient();

private:
    void handleConfig();
    void handleSettingsUpdate();
    void handleMappingUpdate();
    void handleMappingListUpdate();
    void handleTestLed();
    void sendResult(bool success, const std::string &message);

    bool parseUnsignedArg(
        const char *name,
        uint32_t maximum,
        uint32_t &value,
        std::string &error);

    WebServer server_;
    RuntimeSettings *settings_;
    const char *boardName_;
    uint8_t boardRows_;
    ApplySettingsHandler applySettings_;
    TestLedHandler testLed_;
    AllOffHandler allOff_;
};
