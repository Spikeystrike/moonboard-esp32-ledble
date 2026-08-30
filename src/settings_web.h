#pragma once

#include <WebServer.h>

#include <functional>
#include <string>

#include "live_log.h"
#include "ota_auth.h"
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
        LiveLogBuffer *liveLog,
        const char *boardName,
        uint8_t boardRows,
        ApplySettingsHandler applySettings,
        TestLedHandler testLed,
        AllOffHandler allOff);
    void handleClient();

private:
    void handleConfig();
    void handleLogs();
    void handleSettingsUpdate();
    void handleMappingUpdate();
    void handleMappingListUpdate();
    void handleTestLed();
    void handleOtaPage();
    void handleOtaSetup();
    void handleOtaLogin();
    void handleOtaLogout();
    void handleOtaPasswordChange();
    void handleOtaUploadData();
    void handleOtaUploadComplete();
    void sendOtaSetupPage(const std::string &message, int statusCode);
    void sendOtaLoginPage(const std::string &message, int statusCode);
    void sendOtaUploadPage(const std::string &message, int statusCode);
    void sendOtaMessagePage(
        const std::string &title,
        const std::string &message,
        int statusCode);
    void sendOtaHtml(
        const std::string &title,
        const std::string &content,
        int statusCode);
    void beginOtaSession();
    void clearOtaSession();
    bool hasValidOtaSession(bool refresh = true);
    void redirectToOta();
    void sendResult(bool success, const std::string &message);

    bool parseUnsignedArg(
        const char *name,
        uint32_t maximum,
        uint32_t &value,
        std::string &error);

    WebServer server_;
    RuntimeSettings *settings_;
    LiveLogBuffer *liveLog_;
    const char *boardName_;
    uint8_t boardRows_;
    ApplySettingsHandler applySettings_;
    TestLedHandler testLed_;
    AllOffHandler allOff_;
    OtaAuthStore otaAuth_;
    std::string otaSetupToken_;
    std::string otaSessionToken_;
    uint32_t otaSessionLastSeenMs_;
    bool otaUploadAuthorized_;
    bool otaUploadStarted_;
    bool otaUploadSucceeded_;
    std::string otaUploadError_;
    size_t otaUploadSize_;
    bool otaRestartPending_;
    uint32_t otaRestartStartedAtMs_;
};
