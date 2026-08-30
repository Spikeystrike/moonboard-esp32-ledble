# MoonBoard BLE to WLED Bridge for Olimex ESP32-POE-ISO

This fork runs on an
[Olimex ESP32-POE-ISO](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware).
It keeps the MoonBoard-compatible BLE interface from the original project, but
does not drive a local LED data pin. Instead, it sends the selected holds over
wired Ethernet to one WLED controller through WLED's JSON API.

```text
MoonBoard app -- BLE --> Olimex ESP32-POE-ISO -- Ethernet/HTTP --> WLED --> LEDs
```

The WLED output follows the approach used in
[Spikeystrike/cruxwledbridge](https://github.com/Spikeystrike/cruxwledbridge):
each route update sends a complete segment frame to `/json/state`. The frame
first clears the controller's entire physical LED range to black, then sets
only route and optional kicker pixels as local LED IDs and hexadecimal colors.
This avoids switching an active controller off between routes while ensuring
that every unrelated LED remains dark.

## Supported behavior

- MoonBoard Mini and MoonBoard Standard layouts.
- Nordic UART BLE service with the same UUIDs and `MoonBoard` device name as
  the ESP32 source project.
- MoonBoard v1 and v2 problem messages.
- Start, hand, foot, finish, left, right, and match-hold colors.
- The app's `~D` option for an additional dim light above each hold. Route
  colors take precedence if that position is itself part of the route.
- The app's `~Z*` command for clearing every configured physical LED.
- An optional list of non-MoonBoard LEDs that stays illuminated while a valid
  route is displayed.
- An optional route timeout that switches route, above-hold, and kicker LEDs
  off together. A value of `0` keeps routes on indefinitely.
- DHCP over the Olimex Ethernet port. If Ethernet is temporarily unavailable,
  BLE remains active and the most recent route is rendered after reconnection.
- WLED HTTP output runs in a background task, retains only the newest frame,
  and retries failed transfers without blocking MoonBoard BLE processing.
- A browser-based setup and calibration page served directly by the Olimex.
- A remote live-log page that mirrors firmware messages without requiring a
  USB connection.
- Password-protected firmware updates through the browser after the initial
  USB installation.
- Persistent WLED, brightness, timeout, kicker, and mapping settings stored in
  the ESP32's non-volatile storage (NVS).

## Configuration

Only the MoonBoard layout must be selected in
[`src/config.h`](src/config.h) before flashing. All installation-specific
values can then be changed without rebuilding the firmware.

### Board layout

Select exactly one layout:

```cpp
// #define MOONBOARD_STANDARD
#define MOONBOARD_MINI
```

Switching between Mini and Standard still requires a rebuild because the BLE
protocol's logical LED count is compiled for the selected board. Saved settings
for the other layout are ignored automatically.

### Web setup and persistent settings

After flashing, connect Ethernet/PoE and read the DHCP address from the serial
monitor or the router. Open `http://<olimex-ip>/` in a browser. The page allows
these settings to be changed and saved while the firmware is running:

- WLED hostname or IPv4 address, total physical LED count, and segment ID
- route and above-hold brightness
- automatic route timeout
- optional kicker LED enable switch, color, and physical IDs
- the complete logical-to-physical MoonBoard LED mapping

The settings are written to NVS and survive resets, power loss, and normal
firmware updates. Values in `config.h` are firmware defaults: they are used on
the first start, when saved data is incomplete, or when the selected board
layout changes. Merely editing `config.h` and reflashing does not overwrite an
existing compatible NVS configuration.

The page has no login. It should only be reachable from a trusted local
network.

### Remote live log

Open `http://<olimex-ip>/logs` or use the **Open live log** link on the setup
page. The view updates once per second and shows the same application-level
Ethernet, BLE, WLED, calibration, and settings messages that are written to the
USB serial monitor. It can be paused, cleared locally, and configured to stop
automatic scrolling.

All web pages use English by default. The flag button in the upper-right corner
switches between English and German, and the choice is retained in the
browser's local storage. The footer shows firmware version `1.0.0` and the
compile timestamp. The same information is available from `/api/config` and in
the startup log, so the running version can be checked after an OTA restart.

The firmware retains only the most recent 80 messages in a fixed-size RAM ring
buffer. Logs are deliberately not written to flash and are lost when the
Olimex restarts. This avoids flash wear and prevents logging from consuming an
unbounded amount of memory. Like the settings page, the log page has no login
and is intended only for a trusted local network.

### Firmware updates over Ethernet (OTA)

Open `http://<olimex-ip>/ota` or use **Update firmware** on the settings
page. On the first visit, the firmware requires an OTA password with 8 to 64
characters and stores only a salted PBKDF2-SHA-256 verification value in a
separate NVS namespace. The password and all other saved settings survive OTA
updates, power loss, and normal firmware uploads.

Later visits require the password. A successful login creates one random
in-memory browser session that expires after 30 minutes without activity and
is discarded whenever the Olimex restarts. The authenticated page also allows
the OTA password to be changed.

Build the firmware normally and upload only this file:

```text
.pio/build/olimex-esp32-poe-iso/firmware.bin
```

Do not upload `bootloader.bin` or `partitions.bin` on the OTA page. The update
is first written to the inactive application slot and checked before the
Olimex restarts. The current firmware remains bootable if the transfer is
interrupted before completion.

The embedded web server uses HTTP rather than HTTPS, so the password is not
encrypted while travelling over the network. Use OTA only in a trusted local
network. If the password is forgotten, NVS must be erased over USB; this also
removes the saved WLED and LED-mapping settings.

### Calibration mode

The same page contains an LED calibration assistant. Select a logical
MoonBoard position, enter a physical WLED ID, and use **LED testen**. The
selected physical LED lights white for five seconds and every other LED is
cleared. **Zuordnen und speichern** validates the assignment and stores it in
NVS. Previous/next buttons and the displayed MoonBoard coordinate make it
possible to walk through the board in order.

For bulk changes, the full comma-separated mapping can be exported, edited,
validated, and imported. Physical IDs must be unique, inside the configured
WLED range, and distinct from kicker LED IDs. **Alle LEDs aus** immediately
sets the entire configured WLED range to black.

### Physical LED mapping

`LOGICAL_TO_PHYSICAL_LED` defines the initial mapping. After the first save,
the NVS mapping edited on the web page takes precedence. The array index is the
MoonBoard position and its value is the WLED ID. This supports unrelated LEDs
before the MoonBoard, gaps in the LED chain, and a physical order that differs
completely from the standard layout:

```cpp
constexpr uint16_t LOGICAL_TO_PHYSICAL_LED[] = {
    10, // logical position 0 (A1) uses WLED ID 10
    11, // logical position 1 (A2) uses WLED ID 11
    15, // logical position 2 (A3) skips WLED IDs 12-14
    14, // arbitrary order is allowed
    // one entry for every remaining MoonBoard position
};
```

The checked-in example reserves WLED IDs `0-9` before the
MoonBoard, skips one unrelated LED after every MoonBoard column, and leaves a
few IDs after the board. `PHYSICAL_LED_COUNT` is the initial total number of
LEDs configured in WLED, including all LEDs that are not used by the MoonBoard.
It can be changed later on the web page.

Every mapped ID must be smaller than `PHYSICAL_LED_COUNT` and may occur only
once. The firmware validates these rules at startup. LEDs that do not appear in
the mapping remain unused by MoonBoard routes.

### LEDs that are always on with a route

`ROUTE_ALWAYS_ON_LED_IDS` is the initial list of physical WLED IDs that are not
assigned to MoonBoard holds. They switch on whenever a valid route is displayed
and switch off with the route reset command:

```cpp
const bool ROUTE_ALWAYS_ON_LEDS_ENABLED = true;
constexpr uint16_t ROUTE_ALWAYS_ON_LED_IDS[] = {0, 3, 6, 9};
const RgbColor ROUTE_ALWAYS_ON_LED_COLOR(255, 255, 255);
```

The list, enable switch, and color are editable on the web page. The color is
scaled with the route brightness. Each ID must be unique, inside the physical
WLED range, and absent from the MoonBoard mapping; invalid values are rejected.

### Automatic route timeout

`ROUTE_TIMEOUT_MINUTES` is the firmware default. The active value can be edited
on the web page and starts counting whenever a valid route is selected.
Selecting another route restarts the timer. When it expires, every LED on the
configured WLED controller is set to black, including above-hold and kicker
LEDs:

```cpp
const uint16_t ROUTE_TIMEOUT_MINUTES = 15;
```

Set the value to `0` to leave the selected route on indefinitely. The timeout
also continues while Ethernet is temporarily unavailable; an expired route is
therefore not restored after reconnection.

### WLED target

Replace the documentation address `192.0.2.10` on the web page. Plain hostnames
and IPv4 addresses are accepted; WLED must be reachable by unencrypted HTTP
from the Olimex Ethernet network. The configured segment is resized to the
complete physical LED range on every route update. A full frame first clears
all physical LEDs and then sets only route and optional kicker LEDs.

Before each individual-pixel frame, the bridge sends WLED a separate
on/brightness request and waits 100 ms for it to take effect. An "all LEDs
off" operation therefore sends a complete black frame but leaves WLED
logically on. This prevents WLED from dropping the first route frame after it
was switched off.

Network transmission runs in a separate FreeRTOS task. The main BLE loop only
builds a complete frame and places it in a single latest-frame mailbox. If
another route arrives while WLED is slow or unreachable, it replaces the
pending older frame instead of building a backlog. A failed newest frame is
retried after `WLED_RETRY_DELAY_MS`; any route received during that delay takes
priority immediately. Only the first failure and the later recovery are added
to the live log, so an offline WLED does not fill the log continuously.

Request timeout, retry delay, boot test, calibration brightness, and BLE name
remain firmware defaults in `config.h`. The boot test sends three batched WLED
frames (red, green, and blue), instead of one HTTP request per LED.

## Build and flash

1. Install Visual Studio Code and PlatformIO.
2. Open this repository.
3. Select Mini or Standard in `src/config.h`.
4. Connect the Olimex board by USB for the initial flashing.
5. Build and upload the default `olimex-esp32-poe-iso` environment.
6. Connect Ethernet/PoE. The 115200-baud serial monitor is optional and useful
   for discovering the initial DHCP address or diagnosing startup problems.
7. Open `http://<olimex-ip>/`, configure WLED, and calibrate the mapping.
8. Connect the MoonBoard app to the BLE device named `MoonBoard`.

After the initial USB installation, later application-firmware versions can
be installed through `http://<olimex-ip>/ota` without reconnecting USB.

Command-line equivalents:

```sh
pio run
pio run --target upload
pio device monitor
```

The PlatformIO board ID is `esp32-poe-iso`, and the build is pinned to
PlatformIO Espressif32 6.12.0. `min_spiffs.csv` supplies two application slots
of about 1.9 MB each, which provide both enough room for BLE, Ethernet, HTTP,
and OTA and an inactive target slot for safe updates. The built-in MoonBoard
BLE server accepts both acknowledged writes and writes without response, as
used by current versions of the MoonBoard app. It advertises the ESP32's
maximum ATT MTU so complete route writes from newer app versions are not
limited to the default 20-byte payload. Each received BLE write is summarized
in the live log as `[BLE RX]`; this makes MTU or framing problems visible even
when no complete route has reached the protocol parser yet.

## Tests

The native tests cover BLE message framing, problem parsing, snake-layout
coordinates, above-hold mapping, explicit and invalid LED mappings, runtime
settings and CSV parsing, the bounded live-log buffer and JSON output, route
timeout behavior including `millis()` wraparound, WLED pixel IDs, and
brightness scaling:

```sh
pio test -e native
```

## LED mapping

The standard snake layout starts at the lower-left hold, goes up the first
column, down the second column, and repeats. MoonBoard positions are zero-based:
`A1` is `0`, `A2` is `1`, and so on. The explicit mapping is applied only after
that logical position is decoded.

## Network and power notes

- Give WLED a stable DHCP reservation so its configured address does not
  change.
- WLED, the Olimex, and the client network must be able to reach each other.
- Power the LED strings from a correctly sized supply and inject power as
  required. The Olimex no longer carries the LED data or LED power load.
- The WLED JSON API uses segment individual-pixel control, so the configured
  segment is returned to the Solid effect for each route.

## Upstream

This repository was forked from
[labs-tibox/moonboard-esp32-ledble](https://github.com/labs-tibox/moonboard-esp32-ledble).
The BLE protocol processing and original color semantics are retained; the
local FastLED output has been replaced with Olimex Ethernet and WLED output.
