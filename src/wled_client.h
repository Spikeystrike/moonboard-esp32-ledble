#pragma once

#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <string>
#include <vector>

#include "bridge_types.h"

class WledClient
{
public:
    WledClient(
        const WledControllerConfig *controllers,
        size_t controllerCount,
        uint16_t timeoutMs,
        uint16_t wakeDelayMs,
        uint16_t retryDelayMs);

    bool begin();
    bool validateConfiguration() const;
    bool reset();
    bool render(
        const RgbColor *leds,
        size_t ledCount,
        uint8_t brightnessPercent);

private:
    struct ControllerFrame
    {
        std::string host;
        std::string payload;
    };

    struct OutputFrame
    {
        std::vector<ControllerFrame> controllers;
    };

    enum class SendResult
    {
        Success,
        Failed,
        Superseded,
    };

    static void workerEntry(void *context);
    void runWorker();
    bool enqueue(std::vector<ControllerFrame> &&controllers);
    bool takeLatest(OutputFrame &frame);
    bool hasPendingFrame() const;
    bool waitForNewFrame(uint16_t waitMs) const;
    SendResult sendOutputFrame(
        const OutputFrame &frame,
        bool logFailure) const;
    SendResult sendPixelFrame(
        const ControllerFrame &controller,
        bool logFailure) const;
    bool postJson(
        const ControllerFrame &controller,
        const char *payload,
        bool logFailure) const;
    bool postJson(
        const ControllerFrame &controller,
        const std::string &payload,
        bool logFailure) const;

    const WledControllerConfig *controllers_;
    size_t controllerCount_;
    uint16_t timeoutMs_;
    uint16_t wakeDelayMs_;
    uint16_t retryDelayMs_;
    mutable SemaphoreHandle_t frameMutex_;
    TaskHandle_t workerTask_;
    OutputFrame pendingFrame_;
    bool pendingFrameAvailable_;
};
