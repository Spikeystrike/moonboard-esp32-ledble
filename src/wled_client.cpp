#include "wled_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#include <string>

#include "app_log.h"
#include "wled_payload.h"

namespace
{
String stateUrl(const char *host)
{
    String url(host);
    url.trim();
    if (!url.startsWith("http://"))
        url = "http://" + url;
    while (url.endsWith("/"))
        url.remove(url.length() - 1);
    return url + "/json/state";
}
} // namespace

WledClient::WledClient(
    const WledControllerConfig *controllers,
    size_t controllerCount,
    uint16_t timeoutMs,
    uint16_t wakeDelayMs)
    : controllers_(controllers),
      controllerCount_(controllerCount),
      timeoutMs_(timeoutMs),
      wakeDelayMs_(wakeDelayMs)
{
}

bool WledClient::validateConfiguration() const
{
    if (controllers_ == nullptr || controllerCount_ == 0)
    {
        appLogLine("[WLED] No controllers configured");
        return false;
    }

    bool valid = true;
    for (size_t index = 0; index < controllerCount_; ++index)
    {
        const WledControllerConfig &controller = controllers_[index];
        if (
            controller.host == nullptr ||
            controller.host[0] == '\0' ||
            controller.lastGlobalLed < controller.firstGlobalLed)
        {
            appLogPrintf("[WLED] Invalid controller at index %u\n", index);
            valid = false;
        }

        for (size_t other = index + 1; other < controllerCount_; ++other)
        {
            const WledControllerConfig &candidate = controllers_[other];
            const bool overlaps =
                controller.firstGlobalLed <= candidate.lastGlobalLed &&
                candidate.firstGlobalLed <= controller.lastGlobalLed;
            if (overlaps)
            {
                appLogPrintf(
                    "[WLED] Controller ranges %u and %u overlap\n",
                    index,
                    other);
                valid = false;
            }
        }
    }
    return valid;
}

bool WledClient::postJson(
    const WledControllerConfig &controller,
    const char *payload) const
{
    return postJson(controller, std::string(payload));
}

bool WledClient::postJson(
    const WledControllerConfig &controller,
    const std::string &payload) const
{
    const String url = stateUrl(controller.host);
    WiFiClient networkClient;
    HTTPClient http;
    http.setTimeout(timeoutMs_);

    if (!http.begin(networkClient, url))
    {
        appLogPrintf("[WLED] Could not open %s\n", url.c_str());
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    const int status = http.POST(
        reinterpret_cast<uint8_t *>(
            const_cast<char *>(payload.data())),
        payload.size());
    http.end();

    if (status < 200 || status >= 300)
    {
        appLogPrintf(
            "[WLED] POST %s failed with status %d\n",
            url.c_str(),
            status);
        return false;
    }
    return true;
}

bool WledClient::reset()
{
    bool success = true;
    for (size_t index = 0; index < controllerCount_; ++index)
    {
        const WledControllerConfig &controller = controllers_[index];
        const std::string payload = buildWledClearPayload(controller);
        if (payload.empty() || !sendPixelFrame(controller, payload))
            success = false;
    }
    return success;
}

bool WledClient::sendPixelFrame(
    const WledControllerConfig &controller,
    const std::string &payload) const
{
    if (payload.empty())
        return false;

    // WLED documents that on/brightness must already be applied before an
    // individual-pixel request. The HTTP response can arrive before that
    // state change has fully settled, so leave a short gap before the frame.
    const bool wakeSucceeded = postJson(
        controller,
        "{\"on\":true,\"bri\":255,\"transition\":0}");
    if (wakeSucceeded && wakeDelayMs_ > 0)
        delay(wakeDelayMs_);

    const bool frameSucceeded = postJson(controller, payload);
    return wakeSucceeded && frameSucceeded;
}

bool WledClient::render(
    const RgbColor *leds,
    size_t ledCount,
    uint8_t brightnessPercent)
{
    bool success = true;
    for (size_t index = 0; index < controllerCount_; ++index)
    {
        const WledControllerConfig &controller = controllers_[index];
        size_t litLedCount = 0;
        const std::string payload = buildWledStatePayload(
            leds,
            ledCount,
            controller,
            brightnessPercent,
            litLedCount);
        if (payload.empty() || !sendPixelFrame(controller, payload))
            success = false;
    }
    return success;
}
