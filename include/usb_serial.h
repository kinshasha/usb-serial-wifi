#pragma once
#include <Arduino.h>
#include <Usb.h>
bool usb_begin();
void usb_poll();
bool usb_ready();
const char *usb_driver();
bool usb_driver_mode_switch_supported();
bool usb_set_driver_mode(const char *mode);
uint8_t usb_state();
uint8_t usb_send(uint16_t count, uint8_t *data);
uint8_t usb_receive(uint16_t *count, uint8_t *data);
