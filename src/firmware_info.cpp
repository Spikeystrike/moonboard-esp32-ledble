#include "firmware_info.h"

// Increment this value for every firmware release so the running version can
// be verified after an OTA update. The build timestamp distinguishes local
// rebuilds of the same release.
const char FIRMWARE_VERSION[] = "1.3.0";
const char FIRMWARE_BUILD_TIMESTAMP[] = __DATE__ " " __TIME__;
