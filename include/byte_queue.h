#pragma once
#include <stddef.h>
#include <stdint.h>

template <size_t N> class ByteQueue {
  uint8_t data_[N];
  size_t head_ = 0, count_ = 0;
public:
  size_t size() const { return count_; }
  size_t free() const { return N - count_; }
  void clear() { head_ = count_ = 0; }
  bool push(uint8_t b) {
    if (!free()) return false;
    data_[(head_ + count_) % N] = b;
    ++count_;
    return true;
  }
  size_t peek(uint8_t *out, size_t len) const {
    if (len > count_) len = count_;
    for (size_t i = 0; i < len; ++i) out[i] = data_[(head_ + i) % N];
    return len;
  }
  void drop(size_t len) {
    if (len > count_) len = count_;
    head_ = (head_ + len) % N;
    count_ -= len;
  }
};

// Only consume the input byte when its complete expansion fits.
class LineEnding {
  bool previous_cr_ = false;
public:
  void reset() { previous_cr_ = false; }
  template <size_t N> bool append(ByteQueue<N> &q, uint8_t b, bool crlf) {
    bool add_cr = crlf && b == '\n' && !previous_cr_;
    if (q.free() < (add_cr ? 2u : 1u)) return false;
    if (add_cr) q.push('\r');
    q.push(b);
    previous_cr_ = b == '\r';
    return true;
  }
};
