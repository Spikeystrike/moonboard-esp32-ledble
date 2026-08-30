#pragma once

#include <stdint.h>

bool routeTimeoutExpired(
    bool routeActive,
    uint16_t timeoutMinutes,
    uint32_t routeStartedAtMs,
    uint32_t nowMs);
