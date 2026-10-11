#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define HEX 16
class Print {
public:
  template <typename T> void print(T) {}
  template <typename T> void print(T, int) {}
  template <typename T> void println(T) {}
  template <typename T> void println(T, int) {}
  void println() {}
};
class SerialFake : public Print {
public:
  void begin(int) {}
  int available() { return 0; }
  int read() { return -1; }
};
static SerialFake Serial;
static uint32_t fake_millis = 0;
inline uint32_t millis() { return fake_millis; }
inline void delay(uint32_t t) { fake_millis += t; }
inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
