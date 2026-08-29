#include "wled_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#include <string>

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
    uint16_t timeoutMs)
    : controllers_(controllers),
      controllerCount_(controllerCount),
      timeoutMs_(timeoutMs)
{
}

bool WledClient::validateConfiguration() const
{
    if (controllers_ == nullptr || controllerCount_ == 0)
    {
        Serial.println("[WLED] No controllers configured");
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
            Serial.printf("[WLED] Invalid controller at index %u\n", index);
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
                Serial.printf(
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
        Serial.printf("[WLED] Could not open %s\n", url.c_str());
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
        Serial.printf(
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
        if (!postJson(controllers_[index], "{\"on\":false,\"bri\":255}"))
            success = false;
    }
    return success;
}

bool WledClient::render(
    const RgbColor *leds,
    size_t ledCount,
    uint8_t brightnessPercent)
{
    bool success = reset();
    for (size_t index = 0; index < controllerCount_; ++index)
    {
        size_t litLedCount = 0;
        const std::string payload = buildWledStatePayload(
            leds,
            ledCount,
            controllers_[index],
            brightnessPercent,
            litLedCount);
        if (litLedCount == 0)
            continue;

        if (!postJson(controllers_[index], payload))
            success = false;
    }
    return success;
}
