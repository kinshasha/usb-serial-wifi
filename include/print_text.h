#pragma once
#include <stddef.h>
#include <stdint.h>

// A complete web submission is staged before it can become a print job.
// Only printable ASCII reaches the printer; tabs expand to 8-column stops,
// and CR, LF and CRLF all become exactly one CRLF.
template <size_t N> class PrintText {
  uint8_t data_[N];
  size_t used_ = 0, offset_ = 0, column_ = 0, removed_ = 0;
  bool previous_cr_ = false, finished_ = false;
  bool put(uint8_t b) {
    if (used_ == N) return false;
    data_[used_++] = b; return true;
  }
  bool newline() {
    if (N - used_ < 2) return false;
    put('\r'); put('\n'); column_ = 0; return true;
  }
public:
  void reset() { used_ = offset_ = column_ = removed_ = 0; previous_cr_ = finished_ = false; }
  size_t size() const { return used_; }
  size_t removed() const { return removed_; }
  size_t remaining() const { return used_ - offset_; }
  bool append(uint8_t b) {
    if (finished_) return false;
    if (b == '\n' && previous_cr_) { previous_cr_ = false; return true; }
    // Stripped bytes do not split a CRLF pair.
    if (b != '\r' && b != '\n' && b != '\t' && (b < 0x20 || b > 0x7e)) {
      ++removed_; return true;
    }
    previous_cr_ = b == '\r';
    if (b == '\r' || b == '\n') return newline();
    if (b == '\t') {
      size_t spaces = 8 - column_ % 8;
      if (N - used_ < spaces) return false;
      while (spaces--) { put(' '); ++column_; }
      return true;
    }
    if (!put(b)) return false;
    ++column_; return true;
  }
  bool finish() {
    if (finished_ || !used_) return false;
    if (data_[used_-1] != '\n' && !newline()) return false;
    finished_ = true; return true;
  }
  // Staged bytes are not visible to the sending side until finish succeeds.
  size_t peek(uint8_t *out, size_t count) const {
    if (!finished_) return 0;
    if (count > remaining()) count = remaining();
    for (size_t i = 0; i < count; ++i) out[i] = data_[offset_ + i];
    return count;
  }
  void drop(size_t count) {
    if (!finished_) return;
    if (count > remaining()) count = remaining();
    offset_ += count;
  }
};

// Bounds are checked before multiplication, including hostile long headers.
inline bool print_parse_length(const char *begin, const char *end, size_t limit, size_t *out) {
  while (begin < end && (*begin == ' ' || *begin == '\t')) ++begin;
  if (begin == end || !out) return false;
  size_t n = 0;
  for (; begin < end; ++begin) {
    if (*begin < '0' || *begin > '9') return false;
    size_t digit = *begin - '0';
    if (digit > limit || n > (limit - digit) / 10) return false;
    n = n * 10 + digit;
  }
  *out = n; return true;
}
