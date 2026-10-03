# Validation — initial drop

## Completed on 2026-10-03

- Cross-compiled all four environments for the actual UNO R4 WiFi using PlatformIO 6.2.0, Renesas platform 1.8.0, USB Host Shield 2.0 tag 1.7.0 and ArduinoMDNS 1.0.1.
- Native queue tests passed with C++11 and warnings treated as errors: bounds, wraparound, partial writes, atomic CRLF expansion and raw control-byte preservation.
- Native setup tests passed: URL encoding including spaces and special characters, malformed requests, duplicate fields, length limits, password validation and the association/IP/SSID/stability gate.
- Inspected the Renesas EEPROM implementation: backup records reside in separate 1024-byte physical erase blocks. Checksums and read-back verification guard credential commits.

## Hardware acceptance still required

1. Run upstream board_qc with the purchased shield; verify power and SPI before connecting the printer.
2. Confirm actual printer revision, USB descriptors or RS-232 interface, adapter chip and serial cable/handshake.
3. Test scanning while on the setup AP, correct/incorrect passwords, hidden SSIDs, special characters and DHCP failure. Verify failed candidates preserve the previously saved network across a power cycle.
4. Test router loss/recovery, automatic setup fallback, short cancel, long setup and boot reset. Include initial secrets.h credentials in reset testing.
5. Interrupt power during credential saving and verify recovery from the other flash record. Physical power-loss behaviour is not proven by compilation.
6. Print plain text and control sequences; test offline/paper-out, USB unplug/replug, multiple senders and optional XON/XOFF. Cancel cannot retract data already accepted by the printer.

Compilation and native helper tests do not establish real Wi-Fi, USB, printer or power-loss reliability. This is a first hardware-testable release, with conservative one-byte USB writes; throughput tuning follows validation.
