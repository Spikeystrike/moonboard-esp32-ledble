#pragma once

#include "live_log.h"

void appLogLine(const char *message);
void appLogPrintf(const char *format, ...);
LiveLogBuffer &appLogBuffer();
