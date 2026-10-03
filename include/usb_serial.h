#pragma once
#include <Arduino.h>
#include <Usb.h>
bool usb_begin();
void usb_poll();
bool usb_ready();
const char *usb_driver();
uint8_t usb_state();
uint8_t usb_send(uint16_t count, uint8_t *data);
uint8_t usb_receive(uint16_t *count, uint8_t *data);
