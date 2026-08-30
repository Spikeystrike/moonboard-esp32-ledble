#include "wled_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#include <string>
#include <utility>
#include <vector>

#include "app_log.h"
#include "wled_payload.h"

namespace
{
constexpr uint32_t WLED_WORKER_STACK_SIZE = 8192;
constexpr UBaseType_t WLED_WORKER_PRIORITY = 1;

String stateUrl(const std::string &host)
{
    String url(host.c_str());
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
    uint16_t wakeDelayMs,
    uint16_t retryDelayMs)
    : controllers_(controllers),
      controllerCount_(controllerCount),
      timeoutMs_(timeoutMs),
      wakeDelayMs_(wakeDelayMs),
      retryDelayMs_(retryDelayMs),
      frameMutex_(nullptr),
      workerTask_(nullptr),
      pendingFrame_{},
      pendingFrameAvailable_(false)
{
}

bool WledClient::begin()
{
    if (workerTask_ != nullptr)
        return true;

    frameMutex_ = xSemaphoreCreateMutex();
    if (frameMutex_ == nullptr)
    {
        appLogLine("[WLED] Could not create output mutex");
        return false;
    }

    if (
        xTaskCreate(
            workerEntry,
            "wled-output",
            WLED_WORKER_STACK_SIZE,
            this,
            WLED_WORKER_PRIORITY,
            &workerTask_) != pdPASS)
    {
        vSemaphoreDelete(frameMutex_);
        frameMutex_ = nullptr;
        workerTask_ = nullptr;
        appLogLine("[WLED] Could not start output task");
        return false;
    }

    appLogLine("[WLED] Asynchronous latest-frame output task started");
    return true;
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
    const ControllerFrame &controller,
    const char *payload,
    bool logFailure) const
{
    return postJson(
        controller,
        std::string(payload == nullptr ? "" : payload),
        logFailure);
}

bool WledClient::postJson(
    const ControllerFrame &controller,
    const std::string &payload,
    bool logFailure) const
{
    const String url = stateUrl(controller.host);
    WiFiClient networkClient;
    HTTPClient http;
    http.setTimeout(timeoutMs_);

    if (!http.begin(networkClient, url))
    {
        if (logFailure)
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
        if (logFailure)
        {
            appLogPrintf(
                "[WLED] POST %s failed with status %d\n",
                url.c_str(),
                status);
        }
        return false;
    }
    return true;
}

bool WledClient::reset()
{
    if (controllers_ == nullptr || controllerCount_ == 0)
        return false;

    std::vector<ControllerFrame> frames;
    frames.reserve(controllerCount_);
    for (size_t index = 0; index < controllerCount_; ++index)
    {
        const WledControllerConfig &controller = controllers_[index];
        if (controller.host == nullptr || controller.host[0] == '\0')
            return false;
        const std::string payload = buildWledClearPayload(controller);
        if (payload.empty())
            return false;
        frames.push_back({controller.host, payload});
    }
    return enqueue(std::move(frames));
}

bool WledClient::render(
    const RgbColor *leds,
    size_t ledCount,
    uint8_t brightnessPercent)
{
    if (controllers_ == nullptr || controllerCount_ == 0)
        return false;

    std::vector<ControllerFrame> frames;
    frames.reserve(controllerCount_);
    for (size_t index = 0; index < controllerCount_; ++index)
    {
        const WledControllerConfig &controller = controllers_[index];
        if (controller.host == nullptr || controller.host[0] == '\0')
            return false;
        size_t litLedCount = 0;
        const std::string payload = buildWledStatePayload(
            leds,
            ledCount,
            controller,
            brightnessPercent,
            litLedCount);
        if (payload.empty())
            return false;
        frames.push_back({controller.host, payload});
    }
    return enqueue(std::move(frames));
}

bool WledClient::enqueue(std::vector<ControllerFrame> &&controllers)
{
    if (
        controllers.empty() ||
        frameMutex_ == nullptr ||
        workerTask_ == nullptr)
    {
        return false;
    }

    if (xSemaphoreTake(frameMutex_, portMAX_DELAY) != pdTRUE)
        return false;
    pendingFrame_.controllers = std::move(controllers);
    pendingFrameAvailable_ = true;
    xSemaphoreGive(frameMutex_);

    xTaskNotifyGive(workerTask_);
    return true;
}

bool WledClient::takeLatest(OutputFrame &frame)
{
    if (frameMutex_ == nullptr)
        return false;
    if (xSemaphoreTake(frameMutex_, portMAX_DELAY) != pdTRUE)
        return false;
    const bool available = pendingFrameAvailable_;
    if (available)
    {
        frame = std::move(pendingFrame_);
        pendingFrameAvailable_ = false;
    }
    xSemaphoreGive(frameMutex_);
    return available;
}

bool WledClient::hasPendingFrame() const
{
    if (frameMutex_ == nullptr)
        return false;
    if (xSemaphoreTake(frameMutex_, portMAX_DELAY) != pdTRUE)
        return false;
    const bool available = pendingFrameAvailable_;
    xSemaphoreGive(frameMutex_);
    return available;
}

bool WledClient::waitForNewFrame(uint16_t waitMs) const
{
    // Discard notifications already consumed through takeLatest(). The frame
    // itself remains the source of truth, so a concurrent enqueue cannot be
    // lost between this drain, the pending check, and the timed wait.
    ulTaskNotifyTake(pdTRUE, 0);
    if (hasPendingFrame())
        return true;
    if (waitMs == 0)
        return false;

    TickType_t ticks = pdMS_TO_TICKS(waitMs);
    if (ticks == 0)
        ticks = 1;
    ulTaskNotifyTake(pdTRUE, ticks);
    return hasPendingFrame();
}

WledClient::SendResult WledClient::sendPixelFrame(
    const ControllerFrame &controller,
    bool logFailure) const
{
    if (controller.payload.empty())
        return SendResult::Failed;
    if (hasPendingFrame())
        return SendResult::Superseded;

    // WLED requires on/brightness to be committed before individual pixels.
    const bool wakeSucceeded = postJson(
        controller,
        "{\"on\":true,\"bri\":255,\"transition\":0}",
        logFailure);
    if (hasPendingFrame())
        return SendResult::Superseded;
    if (!wakeSucceeded)
        return SendResult::Failed;

    if (
        wakeDelayMs_ > 0 &&
        waitForNewFrame(wakeDelayMs_))
    {
        return SendResult::Superseded;
    }

    const bool frameSucceeded = postJson(
        controller,
        controller.payload,
        logFailure);
    if (hasPendingFrame())
        return SendResult::Superseded;
    return frameSucceeded ? SendResult::Success : SendResult::Failed;
}

WledClient::SendResult WledClient::sendOutputFrame(
    const OutputFrame &frame,
    bool logFailure) const
{
    if (frame.controllers.empty())
        return SendResult::Failed;
    for (const ControllerFrame &controller : frame.controllers)
    {
        const SendResult result = sendPixelFrame(controller, logFailure);
        if (result != SendResult::Success)
            return result;
    }
    return SendResult::Success;
}

void WledClient::workerEntry(void *context)
{
    static_cast<WledClient *>(context)->runWorker();
}

void WledClient::runWorker()
{
    OutputFrame current = {};
    bool haveCurrent = false;
    bool communicationFailed = false;

    while (true)
    {
        if (!haveCurrent)
        {
            while (!takeLatest(current))
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            haveCurrent = true;
        }

        const SendResult result = sendOutputFrame(
            current,
            !communicationFailed);

        OutputFrame latest = {};
        if (takeLatest(latest))
        {
            current = std::move(latest);
            haveCurrent = true;
            continue;
        }

        if (result == SendResult::Success)
        {
            if (communicationFailed)
                appLogLine("[WLED] Communication recovered");
            communicationFailed = false;
            haveCurrent = false;
            continue;
        }

        if (result == SendResult::Superseded)
        {
            haveCurrent = false;
            continue;
        }

        if (!communicationFailed)
        {
            appLogPrintf(
                "[WLED] Output failed; retrying the latest frame in %u ms\n",
                retryDelayMs_);
        }
        communicationFailed = true;

        if (waitForNewFrame(retryDelayMs_) && takeLatest(latest))
            current = std::move(latest);
        haveCurrent = true;
    }
}
