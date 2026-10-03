# OKI MICROLINE 390/391 bring-up

Target family, not a verified exact revision. Check the rear label and installed ports before choosing a build.

Primary reference: [OKI 390/391 Turbo Printer Handbook, parallel and USB standard version](https://www.oki.com/us/printing/download/390T_USB_HBR1_63084.pdf?id=42089404EE).

## Direct USB

The handbook documents a USB-B connector. The proposed connection is UNO shield USB-A -> USB-B printer cable -> OKI. Use `uno_r4_printer`. Confirm the actual device exposes Printer Class 07, subclass 01, protocol 01 or 02 and a bulk OUT endpoint. Physical acceptance remains untested.

No USB serial adapter or baud setting applies to direct USB printing. Send CRLF-delimited ASCII initially, with paper loaded and printer selected. The handbook describes Epson LQ and IBM emulations; match its menu to any command stream you send. Plain ASCII is the first test; no thermal-printer ESC/POS commands are injected.

## Optional RS-232

Use a supported USB-to-RS-232 adapter, the printer's installed optional serial board, and the cable specified for that board. Connector gender alone is insufficient to determine straight-through/null-modem wiring. Verify pin assignments and handshake lines from the board's manual.

The handbook's serial default baud is 9600, parity None and data size 8 bits. Its default Ready/Busy protocol needs a compatible busy signal and cable, which this initial generic bridge does not implement. For the first serial printer test, select X-On/X-Off on the printer and set `BRIDGE_XON_XOFF` to 1 in config.h. Match the rest of the serial framing. Validate printer busy/paper-out behaviour before longer jobs.

## Acceptance sequence

1. Verify shield with `board_qc` and a known USB device.
2. Record printer/adapter USB VID, PID and interface descriptors.
3. Flash matching build; check console/status for USB ready.
4. Send `A\r\n` only. Confirm one line and proper carriage return.
5. Send several lines; test pause/offline and paper-out handling.
6. Disconnect USB during a job. Reconnect and verify stale work is discarded.
7. Test a second TCP sender and a long input stream for ordering/buffering.
8. Verify form feed and printer emulation before sending formatting commands.

If the printer lacks both native USB and a serial board, a USB-to-parallel cable may expose Printer Class, but the exact cable/controller needs descriptor and handshake testing. This firmware is not a GPIO Centronics implementation.
