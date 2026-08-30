#include "live_log.h"

#include <cstdio>
#include <cstring>

namespace
{
void appendJsonString(std::string &json, const char *value)
{
    json += '"';
    if (value != nullptr)
    {
        while (*value != '\0')
        {
            const unsigned char character =
                static_cast<unsigned char>(*value++);
            switch (character)
            {
            case '"':
                json += "\\\"";
                break;
            case '\\':
                json += "\\\\";
                break;
            case '\b':
                json += "\\b";
                break;
            case '\f':
                json += "\\f";
                break;
            case '\n':
                json += "\\n";
                break;
            case '\r':
                json += "\\r";
                break;
            case '\t':
                json += "\\t";
                break;
            default:
                if (character < 0x20)
                {
                    char escaped[7];
                    std::snprintf(
                        escaped,
                        sizeof(escaped),
                        "\\u%04X",
                        static_cast<unsigned int>(character));
                    json += escaped;
                }
                else
                {
                    json += static_cast<char>(character);
                }
            }
        }
    }
    json += '"';
}
} // namespace

LiveLogBuffer::LiveLogBuffer()
    : firstEntry_(0),
      entryCount_(0),
      nextSequence_(1)
{
    std::memset(entries_, 0, sizeof(entries_));
}

void LiveLogBuffer::append(uint32_t timestampMs, const char *message)
{
    size_t entryIndex = 0;
    if (entryCount_ < LIVE_LOG_ENTRY_CAPACITY)
    {
        entryIndex = (firstEntry_ + entryCount_) % LIVE_LOG_ENTRY_CAPACITY;
        ++entryCount_;
    }
    else
    {
        entryIndex = firstEntry_;
        firstEntry_ = (firstEntry_ + 1) % LIVE_LOG_ENTRY_CAPACITY;
    }

    LiveLogEntry &entry = entries_[entryIndex];
    entry.sequence = nextSequence_++;
    if (nextSequence_ == 0)
        nextSequence_ = 1;
    entry.timestampMs = timestampMs;
    std::strncpy(
        entry.message,
        message == nullptr ? "" : message,
        sizeof(entry.message));
    entry.message[sizeof(entry.message) - 1] = '\0';
}

size_t LiveLogBuffer::size() const
{
    return entryCount_;
}

uint32_t LiveLogBuffer::latestSequence() const
{
    if (entryCount_ == 0)
        return 0;
    const size_t index =
        (firstEntry_ + entryCount_ - 1) % LIVE_LOG_ENTRY_CAPACITY;
    return entries_[index].sequence;
}

std::string LiveLogBuffer::jsonSince(uint32_t afterSequence) const
{
    const uint32_t latest = latestSequence();
    if (afterSequence > latest)
        afterSequence = 0;

    std::string json;
    json.reserve(48 + entryCount_ * (LIVE_LOG_MESSAGE_LENGTH + 40));
    json = "{\"next\":" + std::to_string(latest) +
        ",\"entries\":[";
    bool first = true;
    for (size_t offset = 0; offset < entryCount_; ++offset)
    {
        const LiveLogEntry &entry = entries_[
            (firstEntry_ + offset) % LIVE_LOG_ENTRY_CAPACITY];
        if (entry.sequence <= afterSequence)
            continue;

        if (!first)
            json += ',';
        first = false;
        json += "{\"seq\":" + std::to_string(entry.sequence);
        json += ",\"ms\":" + std::to_string(entry.timestampMs);
        json += ",\"text\":";
        appendJsonString(json, entry.message);
        json += '}';
    }
    json += "]}";
    return json;
}
