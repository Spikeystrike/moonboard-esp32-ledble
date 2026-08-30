#include "app_log.h"

#include <Arduino.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
LiveLogBuffer logBuffer;

void publish(const char *message)
{
    Serial.println(message);
    logBuffer.append(static_cast<uint32_t>(millis()), message);
}
} // namespace

void appLogLine(const char *message)
{
    publish(message == nullptr ? "" : message);
}

void appLogPrintf(const char *format, ...)
{
    char message[256];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    message[sizeof(message) - 1] = '\0';

    size_t length = std::strlen(message);
    while (
        length > 0 &&
        (message[length - 1] == '\r' || message[length - 1] == '\n'))
    {
        message[--length] = '\0';
    }
    publish(message);
}

LiveLogBuffer &appLogBuffer()
{
    return logBuffer;
}
