#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

#include <string>

constexpr size_t LIVE_LOG_ENTRY_CAPACITY = 80;
constexpr size_t LIVE_LOG_MESSAGE_LENGTH = 192;

struct LiveLogEntry
{
    uint32_t sequence;
    uint32_t timestampMs;
    char message[LIVE_LOG_MESSAGE_LENGTH];
};

class LiveLogBuffer
{
public:
    LiveLogBuffer();

    void append(uint32_t timestampMs, const char *message);
    size_t size() const;
    uint32_t latestSequence() const;
    std::string jsonSince(uint32_t afterSequence) const;

private:
    void lock() const;
    void unlock() const;
    uint32_t latestSequenceUnlocked() const;

#if defined(ARDUINO)
    mutable SemaphoreHandle_t mutex_;
#endif
    LiveLogEntry entries_[LIVE_LOG_ENTRY_CAPACITY];
    size_t firstEntry_;
    size_t entryCount_;
    uint32_t nextSequence_;
};
