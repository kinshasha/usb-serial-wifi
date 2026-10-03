#include "setup_form.h"
#include <assert.h>
#include <stdio.h>
int main() {
  char ssid[33], pass[65];
  assert(form_field("ssid=+Lab%2B%26%25+&password=ab%2Bcd%26ef", "ssid", ssid, sizeof(ssid)));
  assert(!strcmp(ssid, " Lab+&% "));
  assert(form_field("ssid=Lab&password=ab%2Bcd%26ef", "password", pass, sizeof(pass)) && !strcmp(pass,"ab+cd&ef"));
  assert(form_field("ssid=Open&password=", "password", pass, sizeof(pass)) && pass[0] == 0);
  assert(!form_field("ssid=bad%00name", "ssid", ssid, sizeof(ssid)));
  assert(!form_field("ssid=bad%ZZ", "ssid", ssid, sizeof(ssid)));
  assert(!form_field("ssid=bad%", "ssid", ssid, sizeof(ssid)));
  assert(!form_field("ssid=a&ssid=b", "ssid", ssid, sizeof(ssid)));
  assert(!form_field("ssid=123456789012345678901234567890123", "ssid", ssid, sizeof(ssid)));
  assert(form_field("ssid=12345678901234567890123456789012", "ssid", ssid, sizeof(ssid)));
  assert(!form_field("password=abc", "ssid", ssid, sizeof(ssid)));
  assert(personal_password_valid("") && personal_password_valid("12345678"));
  assert(!personal_password_valid("1234567"));
  memset(pass, 'a', 64); pass[64] = 0; assert(personal_password_valid(pass));
  pass[5] = 'z'; assert(!personal_password_valid(pass));
  assert(!valid_saved_network(false, true, true, 2000));
  assert(!valid_saved_network(true, false, true, 2000));
  assert(!valid_saved_network(true, true, false, 2000));
  assert(!valid_saved_network(true, true, true, 1499));
  assert(valid_saved_network(true, true, true, 1500));
  puts("Wi-Fi form and verification-gate tests passed");
}
