#pragma once
#define BRIDGE_HOSTNAME "usbserial"
#define BRIDGE_SETUP_SSID "USB-Serial-Setup"
#define BRIDGE_TCP_PORT 9100
#define BRIDGE_RESET_PIN 2 // optional momentary button to GND; hold at boot
#define BRIDGE_BAUD 9600UL
#define BRIDGE_DATA_BITS 8
#define BRIDGE_PARITY 0 // CDC: 0 none, 1 odd, 2 even
#define BRIDGE_STOP_BITS 0 // CDC: 0 one, 1 one-and-a-half, 2 two
#define BRIDGE_CONTROL_LINES 3 // bit 0 DTR, bit 1 RTS; can reset some devices
#define BRIDGE_CRLF 0 // 0 byte-transparent; 1 add CR before a bare LF
#ifndef BRIDGE_XON_XOFF
#define BRIDGE_XON_XOFF 0 // enable for an OKI RS-232 interface set to X-On/X-Off
#endif
#ifndef BRIDGE_DRIVER
#define BRIDGE_DRIVER 1 // 1 CDC-ACM, 2 FTDI, 3 PL2303, 4 USB Printer Class
#endif
#if BRIDGE_PARITY > 2 || BRIDGE_STOP_BITS > 2 || BRIDGE_DATA_BITS < 5 || BRIDGE_DATA_BITS > 8
#error "Unsupported serial framing"
#endif
