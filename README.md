# MoonBoard BLE to WLED Bridge for Olimex ESP32-POE-ISO

This fork runs on an
[Olimex ESP32-POE-ISO](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware).
It keeps the MoonBoard-compatible BLE interface from the original project, but
does not drive a local LED data pin. Instead, it sends the selected holds over
wired Ethernet to one or more WLED controllers through WLED's JSON API.

```text
MoonBoard app -- BLE --> Olimex ESP32-POE-ISO -- Ethernet/HTTP --> WLED --> LEDs
```

The WLED output follows the approach used in
[Spikeystrike/cruxwledbridge](https://github.com/Spikeystrike/cruxwledbridge):
each update first switches the configured controllers off to clear stale
pixels, then sends the active pixels as local LED IDs and hexadecimal colors to
`/json/state`. Multiple controllers may own non-overlapping inclusive ranges
of global physical LED IDs.

## Supported behavior

- MoonBoard Mini and MoonBoard Standard layouts.
- Nordic UART BLE service with the same UUIDs and `MoonBoard` device name as
  the ESP32 source project.
- MoonBoard v1 and v2 problem messages.
- Start, hand, foot, finish, left, right, and match-hold colors.
- The app's `~D` option for an additional dim light above each hold. Route
  colors take precedence if that position is itself part of the route.
- The app's `~Z*` command for switching all configured WLED controllers off.
- DHCP over the Olimex Ethernet port. If Ethernet is temporarily unavailable,
  BLE remains active and the most recent route is rendered after reconnection.
- Multiple WLED controllers with the same global-to-local LED range model as
  `cruxwledbridge`.

## Configuration

Edit [`src/config.h`](src/config.h) before flashing.

### Board layout

Select exactly one layout:

```cpp
// #define MOONBOARD_STANDARD
#define MOONBOARD_MINI
```

### Physical LED spacing

`LED_OFFSET` maps a logical MoonBoard position to a physical WLED LED ID. With
an offset of `2`, logical positions `0`, `1`, and `2` use physical LED IDs `0`,
`2`, and `4`:

```cpp
const uint8_t LED_OFFSET = 2;
```

WLED must be configured with at least `LOGICAL_LED_COUNT * LED_OFFSET` LEDs.

### WLED controllers

Replace the documentation address `192.0.2.10`. Each range is inclusive and
uses global physical LED IDs. The last number is the WLED segment ID:

```cpp
const WledControllerConfig WLED_CONTROLLERS[] = {
    {"192.168.1.50", 0, 299, 0},
};
```

For two controllers, continue the global numbering and do not overlap ranges:

```cpp
const WledControllerConfig WLED_CONTROLLERS[] = {
    {"192.168.1.50", 0, 149, 0},
    {"192.168.1.51", 150, 299, 0},
};
```

The firmware subtracts each controller's range start before sending local WLED
pixel IDs. Plain hostnames and IPv4 addresses are accepted; WLED must be
reachable by unencrypted HTTP from the Olimex Ethernet network.

Brightness, request timeout, boot test, and BLE name are configured in the
same file. The boot test sends only three batched WLED frames (red, green, and
blue), instead of one HTTP request per LED.

## Build and flash

1. Install Visual Studio Code and PlatformIO.
2. Open this repository.
3. Configure `src/config.h`.
4. Connect the Olimex board by USB for flashing.
5. Build and upload the default `olimex-esp32-poe-iso` environment.
6. Connect Ethernet/PoE and open the serial monitor at 115200 baud.
7. Connect the MoonBoard app to the BLE device named `MoonBoard`.

Command-line equivalents:

```sh
pio run
pio run --target upload
pio device monitor
```

The PlatformIO board ID is `esp32-poe-iso`, and the build is pinned to
PlatformIO Espressif32 6.12.0. `min_spiffs.csv` supplies two larger application
slots because BLE, Ethernet, and HTTP together do not fit the board package's
smaller default slot. The BLESerial dependency is pinned to a commit instead
of tracking a moving branch.

## Tests

The native tests cover BLE message framing, problem parsing, snake-layout
coordinates, above-hold mapping, global-to-local WLED IDs, and brightness
scaling:

```sh
pio test -e native
```

## LED mapping

The standard snake layout starts at the lower-left hold, goes up the first
column, down the second column, and repeats. MoonBoard positions are zero-based:
`A1` is `0`, `A2` is `1`, and so on. `LED_OFFSET` is applied only after that
logical position is decoded.

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
