# USB Serial Wi-Fi

Standalone Wi-Fi to USB bridge for **Arduino UNO R4 WiFi + MAX3421E USB Host Shield 2.0**.
Initial printer target: **OKI MICROLINE 390/391 family**. No KX-Print code or typewriter wiring is required.

This is initial firmware, not yet verified with a physical shield or printer. See `docs/validation.md` for actual checks.

## Choose the connection first

| Connection on your printer | Build environment | Extra hardware |
|---|---|---|
| Native USB-B printer port | `uno_r4_printer` | USB-A to USB-B cable |
| Optional RS-232 port, through an FTDI adapter | `uno_r4_ftdi` | Supported FTDI USB-to-RS-232 adapter and correct printer serial cable |
| Optional RS-232 port, through a PL2303 adapter | `uno_r4_pl2303` | Supported PL2303 adapter and correct serial cable |
| Actual CDC-ACM USB serial device | `uno_r4_acm` | Device's USB cable |

**USB Printer Class and USB serial are different.** The printer build is for a standard unidirectional/bidirectional USB Printer Class interface (protocol 1 or 2). Descriptor confirmation is still required on your actual OKI. It does not implement IEEE 1284.4 protocol 3 or IPP-over-USB.

OKI documents USB and parallel as standard on one 390/391 Turbo revision, with serial as an option. Earlier/regional revisions can differ. A USB socket does not mean the printer is a serial device. See `docs/oki.md`.

The FTDI driver currently targets upstream's single-port VID/PID 0403:6001. The PL2303 driver has upstream's limited chip/revision support. **CH340/CH341 and CP210x are not supported by this release.** No automatic driver selection: flash the matching build.

## Hardware

- UNO R4 WiFi (not UNO R4 Minima, not an ESP32 firmware target).
- Full-size MAX3421E Host Shield 2.0 with 5 V-compatible logic level shifting and its six-pin ICSP socket populated.
- Shield defaults: D9 interrupt, D10 chip select, SPI D11/D12/D13 or ICSP; 5 V, 3.3 V, GND and reset as required by the shield.
- Single CANCEL/RESET button: **D2 to GND**. Tap to cancel queued output; hold for 3 seconds to open setup; hold at power-up for about 5 seconds to erase saved Wi-Fi credentials.
- USB-C powers/programs the UNO. The shield's USB-A connects to the peripheral. The OKI has its own mains power.
- Check the exact clone's power jumpers and schematic before use. Some shield variants also use D7 for reset.
- Do not connect RS-232 electrical signals directly to UNO GPIO or use a TTL UART cable in place of an RS-232 adapter.

## Build and flash

Install PlatformIO. From this directory:

```sh
# Optional: pre-load initial credentials (otherwise use setup AP).
cp include/secrets.h.example include/secrets.h
# Edit secrets.h locally. It is excluded from Git.

# For direct USB to the OKI:
pio run -e uno_r4_printer
pio run -e uno_r4_printer -t upload
pio device monitor -b 115200
```

For USB serial, substitute `uno_r4_acm`, `uno_r4_ftdi` or `uno_r4_pl2303`.
Serial defaults in `include/config.h`: **9600 baud, 8 data bits, no parity, 1 stop bit**, DTR/RTS asserted. Match the connected device; asserting DTR can reset some microcontrollers.

The host library is pinned at 1.7.0, which has UNO R4 support. `lib_compat_mode = off` accommodates its incomplete Renesas PlatformIO metadata. Build the project for the real R4 target; do not force AVR or ESP32 board macros.

## Wi-Fi setup

1. Boot without credentials, or hold CANCEL/RESET for 3 seconds to open setup.
2. Join **USB-Serial-Setup** (open setup network).
3. Open the configuration address printed on the USB-C serial console, usually `http://192.168.4.1/`.
4. Press **Scan networks** to see nearby names, signal strength and security. Select a network or type a hidden SSID, enter its password, and test the connection.
5. The setup AP temporarily disconnects while the UNO joins the candidate network. Authentication/association, a DHCP address, the matching SSID and a stable connection must succeed before saving. No reboot is required.
6. On success, join your normal network and open `http://usbserial.local/status` or the DHCP IP printed in the console. On failure, reconnect to the setup AP to see the error and retry; previous verified credentials remain saved.

Two checksummed credential records occupy separate flash erase blocks, with read-back verification after saving. Reconnection does not rewrite credentials. An initial connection failure opens setup; subsequent Wi-Fi loss retries the saved network three times with backoff before reopening setup. Hold the button at boot for about 5 seconds to clear saved settings, including any compiled initial credentials.

The UNO's WiFiS3 API changes between AP and station modes; this first version does not keep the setup page connected during authentication. Scanning and association are synchronous module operations and can briefly delay button/USB polling. Open and personal-password networks are supported; WEP and enterprise authentication are not. WPA3 compatibility depends on module firmware and must be tested. Internet access is not required or verified.

Wi-Fi settings and raw TCP are unauthenticated: use a trusted LAN, with no Internet port forwarding. The raw bridge is enabled only on the joined network, not the setup AP.

## Mac terminal

Interactive connection (one client at a time):

```sh
nc usbserial.local 9100
```

Type a line, press Return; Control+C disconnects. Plain Terminal input supplies LF. For initial OKI testing, use explicit CRLF:

```sh
printf 'HELLO OKI\r\n' | nc -w 2 usbserial.local 9100
```

`-w 2` closes an idle connection after two seconds; a bidirectional device that continually sends data can keep it alive. Control+C always disconnects interactively. Use the IP if `.local` does not resolve.

Set `BRIDGE_CRLF` to 1 for interactive plain-text printing if the printer needs CRLF. Leave it at 0 for raw printer commands or binary data. ASCII, ESC, NUL and form feed are preserved by default. There is no Unicode/font rendering or PDF/PostScript conversion.

In printer mode, an optional final form feed advances to the next form:

```sh
printf 'HELLO OKI\r\n\f' | nc -w 2 usbserial.local 9100
```

No automatic reset/cut/eject is injected. Serial modes return received device bytes over the same TCP socket; printer mode is initially print-only.

## Buffering and failure behaviour

- One sender; other active senders are closed to avoid mixing print streams.
- Bounded TX/RX queues and TCP backpressure rather than dropping excess input.
- Accepted pending bytes drain after TCP close while the USB device remains connected.
- USB or Wi-Fi loss clears the session/queues. Reconnected devices never receive stale queued work.
- Failed USB sends stop the session without automatic retry: delivery can be uncertain.
- First implementation sends one byte per USB transfer to simplify partial-transfer handling. Throughput optimisation comes after hardware validation.
- Optional software XON/XOFF (`BRIDGE_XON_XOFF=1`) consumes device DC1/DC3, pausing/resuming outbound data. It is off by default; hardware Ready/Busy handshaking is not implemented.
- The upstream USB library has synchronous calls/timeouts. This is not a real-time bridge; an unresponsive device can briefly delay networking.
- `cancel` can discard bridge buffers, but cannot retract bytes already buffered in the printer.

USB-C console at 115200: `status`, `cancel`. The console is diagnostic, not the bridged USB peripheral port.

## Tests

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/queue_test.cpp -o /tmp/queue-test
/tmp/queue-test
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/setup_test.cpp -o /tmp/setup-test
/tmp/setup-test
pio run -e uno_r4_acm -e uno_r4_ftdi -e uno_r4_pl2303 -e uno_r4_printer
```

GitHub Actions contains all four firmware builds and both native test suites. Run the shield library's `board_qc` sketch separately before the first printer test.

## Licence and upstream

Project code: GPL-2.0-or-later to match linking with USB Host Shield 2.0. Vendored USBPrinter files retain their original MIT notices.

- https://github.com/felis/USB_Host_Shield_2.0
- https://github.com/gdsports/USBPrinter_uhs2
- The vendored printer driver places OUT at endpoint-table index 1, allowing a unidirectional table with two entries, and preserves its detected bidirectional flag. Its unchanged status/read API is not used by the first print-only backend.
- The old USBPrinter README's required patch #473 has already been merged upstream.
