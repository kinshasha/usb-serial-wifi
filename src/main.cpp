#include <Arduino.h>
#include <WiFiS3.h>
#include "config.h"
#include "byte_queue.h"
#include "usb_serial.h"
#include "network.h"

static WiFiServer listener(BRIDGE_TCP_PORT);
static WiFiClient client;
static ByteQueue<2048> tx;
static ByteQueue<1024> rx;
static LineEnding endings;
static bool listening = false, session = false, was_ready = false;
static uint32_t bytes_tx = 0, bytes_rx = 0, faults = 0;
static uint8_t last_usb_error = 0;
static bool flow_paused = false;

static void abort_session(const char *reason, uint8_t code = 0) {
  Serial.print("Session stopped: "); Serial.print(reason);
  Serial.print(" (USB code "); Serial.print(code, HEX); Serial.println(")");
  client.stop(); tx.clear(); rx.clear(); endings.reset(); session = false;
  last_usb_error = code; ++faults;
}

void bridge_status(Print &out) {
  out.println("USB Serial Wi-Fi 0.1.0");
  out.print("driver: "); out.println(usb_driver());
  out.print("baud: "); out.println(BRIDGE_BAUD);
  out.print("data/parity/stop codes: "); out.print(BRIDGE_DATA_BITS);
  out.print('/'); out.print(BRIDGE_PARITY); out.print('/'); out.println(BRIDGE_STOP_BITS);
  out.print("usb ready: "); out.println(usb_ready() ? "yes" : "no");
  out.print("usb state: 0x"); out.println(usb_state(), HEX);
  out.print("network ready: "); out.println(network_ready() ? "yes" : "no");
  out.print("Wi-Fi mode: "); out.println(network_mode());
  out.print("Wi-Fi result: "); out.println(network_result());
  out.print("ip: "); out.println(WiFi.localIP());
  out.print("TCP port: "); out.println(BRIDGE_TCP_PORT);
  out.print("session: "); out.println(session ? "active/draining" : "idle");
  out.print("XOFF paused: "); out.println(flow_paused ? "yes" : "no");
  out.print("pending TX/RX: "); out.print(tx.size()); out.print('/'); out.println(rx.size());
  out.print("bytes TX/RX: "); out.print(bytes_tx); out.print('/'); out.println(bytes_rx);
  out.print("faults: "); out.println(faults);
  out.print("last USB error: 0x"); out.println(last_usb_error, HEX);
}

static void bridge_poll() {
  const bool ready = usb_ready();
  if (!ready) flow_paused = false;
  if (was_ready && !ready && session) abort_session("USB disconnected");
  if (was_ready != ready) Serial.println(ready ? "USB serial ready" : "USB serial unavailable");
  was_ready = ready;
  if (!network_ready()) {
    if (session) abort_session("Wi-Fi disconnected");
    if (listening) listener.end();
    listening = false; return;
  }
  if (!listening) { listener.begin(); listening = true; }
  if (!session) {
    WiFiClient incoming = listener.accept();
    if (incoming) {
      if (!ready) { incoming.stop(); return; }
      client = incoming; tx.clear(); rx.clear(); endings.reset(); session = true;
      Serial.println("TCP session accepted");
    }
  } else {
    // accept() returns new connections, unlike available() which may return
    // the existing active client. Reject a second sender.
    WiFiClient incoming = listener.accept();
    if (incoming && incoming != client) incoming.stop();
  }
  if (!ready) return;

  if (session) {
    // Reserve two slots so optional LF expansion is atomic.
    size_t budget = 128;
    while (budget-- && tx.free() >= 2 && client.available()) {
      int b = client.read();
      if (b < 0) break;
      endings.append(tx, static_cast<uint8_t>(b), BRIDGE_CRLF != 0);
    }
    if (tx.size() && !flow_paused) {
      // One byte per USB transfer: prevents a multi-packet transfer from
      // partially succeeding then being replayed. Slow but conservative.
      uint8_t b; tx.peek(&b, 1);
      uint8_t rc = usb_send(1, &b);
      if (rc == hrNAK) {
        // Printers commonly NAK while their endpoint is busy. Keep the byte
        // queued and retry on the next loop instead of aborting the session.
        return;
      }
      if (rc) {
        // Do not retry ambiguous sends: the endpoint may have received data.
        abort_session("USB transmit failed; delivery uncertain", rc); return;
      }
      tx.drop(1); ++bytes_tx;
    }
  }

  // Drain USB input even without a TCP session to avoid stale replies being
  // delivered to the next connection. Request a single 64-byte packet.
  if (!session || rx.free() >= 64) {
    uint8_t data[64]; uint16_t count = sizeof(data);
    uint8_t rc = usb_receive(&count, data);
    if (rc && rc != hrNAK) {
      if (session) abort_session("USB receive failed", rc);
      return;
    }
    if (!rc) {
      for (uint16_t i = 0; i < count; ++i) {
#if BRIDGE_XON_XOFF && BRIDGE_DRIVER != 4
        if (data[i] == 0x13) { flow_paused = true; continue; }
        if (data[i] == 0x11) { flow_paused = false; continue; }
#endif
        if (session && client.connected()) { rx.push(data[i]); ++bytes_rx; }
      }
    }
  }
  if (session && client.connected() && rx.size()) {
    uint8_t data[64]; size_t count = rx.peek(data, sizeof(data));
    size_t sent = client.write(data, count);
    rx.drop(sent); // preserve unsent data after a short socket write
  }
  if (session && !client.connected() && !client.available() && !tx.size()) {
    client.stop(); rx.clear(); session = false;
    Serial.println("TCP session drained");
  }
}

static void console_poll() {
  // USB-C serial is diagnostics, not a second data source.
  static char command[32]; static size_t used = 0;
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\r' || c == '\n') {
      command[used] = 0;
      if (!strcmp(command, "status")) bridge_status(Serial);
      else if (!strcmp(command, "cancel")) abort_session("console cancel");
      else if (used) Serial.println("Commands: status, cancel");
      used = 0;
    } else if (used < sizeof(command) - 1) command[used++] = static_cast<char>(c);
  }
}
static void button_poll() {
  static bool raw = HIGH, stable_button = HIGH, held = false;
  static uint32_t changed = 0, pressed = 0;
  bool now = digitalRead(BRIDGE_RESET_PIN);
  if (now != raw) { raw = now; changed = millis(); }
  if (millis() - changed >= 30 && raw != stable_button) {
    stable_button = raw;
    if (stable_button == LOW) { pressed = millis(); held = false; }
    else if (!held) abort_session("CANCEL button");
  }
  if (stable_button == LOW && !held && millis() - pressed >= 3000) {
    held = true; abort_session("CANCEL/RESET held: opening setup");
    if (listening) listener.end();
    listening = false;
    network_open_setup();
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(BRIDGE_RESET_PIN, INPUT_PULLUP);
  bool reset = true;
  uint32_t start = millis();
  while (millis() - start < 2500) {
    if (digitalRead(BRIDGE_RESET_PIN) != LOW) reset = false;
    delay(5);
  }
  Serial.println("USB Serial Wi-Fi starting");
  if (!usb_begin()) Serial.println("MAX3421E init failed; check shield, ICSP and power");
  network_begin(reset);
}
void loop() {
  button_poll();
  usb_poll();
  bridge_poll();
  console_poll();
  network_poll();
}
