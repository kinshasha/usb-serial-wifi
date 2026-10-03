#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
inline int form_hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
// Reject malformed escapes, embedded NUL, overflow and duplicate fields.
// Preserve SSID spaces and punctuation exactly.
inline bool form_field(const char *body, const char *key, char *out, size_t cap) {
  size_t n = strlen(key); bool found = false;
  while (*body) {
    const char *end = strchr(body, '&');
    if (!end) end = body + strlen(body);
    if (static_cast<size_t>(end - body) > n && !strncmp(body, key, n) && body[n] == '=') {
      if (found) return false;
      found = true; size_t used = 0;
      for (const char *p = body + n + 1; p < end; ++p) {
        char ch = *p;
        if (ch == '+') ch = ' ';
        else if (ch == '%') {
          if (end - p < 3 || form_hex(p[1]) < 0 || form_hex(p[2]) < 0) return false;
          ch = static_cast<char>((form_hex(p[1]) << 4) | form_hex(p[2])); p += 2;
        }
        if (!ch || used + 1 >= cap) return false;
        out[used++] = ch;
      }
      out[used] = 0;
    }
    body = *end ? end + 1 : end;
  }
  return found;
}
inline bool personal_password_valid(const char *password) {
  size_t n = strlen(password);
  if (!n || (n >= 8 && n <= 63)) return true;
  if (n != 64) return false;
  for (size_t i = 0; i < n; ++i) if (form_hex(password[i]) < 0) return false;
  return true;
}
inline bool valid_saved_network(bool associated, bool nonzero_ip, bool matches_ssid, uint32_t stable_ms) {
  return associated && nonzero_ip && matches_ssid && stable_ms >= 1500;
}
