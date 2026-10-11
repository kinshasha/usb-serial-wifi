#pragma once
#include <string.h>
#include "print_text.h"
inline bool print_header_equal_ci(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
    if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
    if (x != y) return false;
  }
  return true;
}
inline bool print_request_headers(const char *request, size_t form_limit, size_t print_limit, int *length) {
  int body_length = 0;
  bool have_length = false, have_print_type = false;
  const bool print_post = !strncmp(request, "POST /print ", 12);
  const size_t length_limit = print_post ? print_limit : form_limit;
  const char *p = strstr(request, "\r\n");
  if (!p) return false;
  p += 2;
  while (*p && strncmp(p, "\r\n", 2)) {
    const char *end = strstr(p, "\r\n");
    if (!end) return false;
    size_t n = end-p;
    if (n >= 15 && print_header_equal_ci(p, "Content-Length:", 15)) {
      if (have_length) return false;
      have_length = true; const char *v = p+15;
      size_t parsed = 0;
      if (!print_parse_length(v, end, length_limit, &parsed)) return false;
      body_length = static_cast<int>(parsed);
    } else if (print_post && n >= 13 && print_header_equal_ci(p, "Content-Type:", 13)) {
      if (have_print_type) return false;
      const char *v = p + 13;
      while (v < end && (*v == ' ' || *v == '\t')) ++v;
      size_t type_length = 0;
      while (v + type_length < end && v[type_length] != ';' && v[type_length] != ' ') ++type_length;
      have_print_type = (type_length == 10 && print_header_equal_ci(v, "text/plain", 10)) ||
                        (type_length == 24 && print_header_equal_ci(v, "application/octet-stream", 24));
      if (!have_print_type) return false;
    } else if (n >= 18 && print_header_equal_ci(p, "Transfer-Encoding:", 18)) return false;
    p = end+2;
  }
  if (!(strncmp(request, "POST ", 5) || have_length) || (print_post && (!have_print_type || body_length <= 0))) return false;
  *length = body_length; return true;
}
