#include "route_timeout.h"

bool routeTimeoutExpired(
    bool routeActive,
    uint16_t timeoutMinutes,
    uint32_t routeStartedAtMs,
    uint32_t nowMs)
{
    if (!routeActive || timeoutMinutes == 0)
        return false;

    const uint32_t timeoutMs =
        static_cast<uint32_t>(timeoutMinutes) * 60UL * 1000UL;
    const uint32_t elapsedMs = nowMs - routeStartedAtMs;
    return elapsedMs >= timeoutMs;
}
