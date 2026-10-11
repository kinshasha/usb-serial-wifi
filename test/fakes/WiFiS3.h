#pragma once
#include "Arduino.h"
class WiFiClient {
public:
  bool connected() const { return false; }
  int available() const { return 0; }
  int read() { return -1; }
  void stop() {}
  size_t write(const uint8_t *, size_t n) { return n; }
  explicit operator bool() const { return false; }
  bool operator!=(const WiFiClient &) const { return false; }
};
class WiFiServer {
public:
  explicit WiFiServer(int) {}
  void begin() {}
  void end() {}
  WiFiClient accept() { return WiFiClient(); }
};
class WiFiFake { public: int localIP() { return 1; } };
static WiFiFake WiFi;
