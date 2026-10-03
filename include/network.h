#pragma once
#include <Arduino.h>
void network_begin(bool reset);
void network_poll();
bool network_ready();
void network_open_setup();
const char *network_mode();
const char *network_result();
void bridge_status(Print &out);
