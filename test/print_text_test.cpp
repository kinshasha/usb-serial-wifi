#include "print_text.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

template <size_t N> static void expect(PrintText<N> &job, const char *text) {
  uint8_t out[N];
  size_t n = job.peek(out, sizeof(out));
  assert(n == strlen(text) && !memcmp(out, text, n));
}
int main() {
  PrintText<128> job;
  const uint8_t input[] = {'A', '\r', '\n', 'B', '\r', 'C', '\n', 'D', '\t', 'E', 0, 0x1b, 0x0c, 0x7f, 0xc3, 0xa9};
  for (uint8_t b : input) assert(job.append(b));
  uint8_t out[128];
  assert(job.peek(out, sizeof(out)) == 0); // No partially received job leaks to USB.
  assert(job.finish());
  expect(job, "A\r\nB\r\nC\r\nD       E\r\n");
  assert(job.removed() == 6);
  assert(!job.append('X') && !job.finish());
  job.drop(3); assert(job.peek(out, 1) == 1 && out[0] == 'B');
  job.drop(1000); assert(job.remaining() == 0);
  job.reset(); assert(!job.finish());
  for (unsigned i = 0; i < 256; ++i) assert(job.append(i));
  assert(job.finish());
  size_t n = job.peek(out, sizeof(out));
  for (size_t i = 0; i < n; ++i) assert(out[i] == '\r' || out[i] == '\n' || (out[i] >= 0x20 && out[i] <= 0x7e));
  PrintText<8> small;
  for (int i = 0; i < 6; ++i) assert(small.append('a'));
  assert(small.finish() && small.size() == 8);
  small.reset();
  for (int i = 0; i < 8; ++i) assert(small.append('a'));
  assert(!small.finish() && !small.peek(out, 1));
  small.reset(); assert(small.append('\t')); assert(!small.append('\t'));
  assert(!small.peek(out, sizeof(out)));
  small.reset(); assert(small.append('x')); assert(small.append('\r')); assert(small.append(0)); assert(small.append('\n'));
  assert(small.finish()); expect(small, "x\r\n");
  small.reset(); assert(small.append(0) && small.append(0xff) && small.append(0x1b)); assert(!small.finish());
  size_t length = 0;
  const char *valid[] = {"8192", " 8192", "\t12", "0"};
  for (const char *s : valid) assert(print_parse_length(s, s+strlen(s), 8192, &length));
  const char *invalid[] = {"", "-1", "+1", "12x", "8193", "9999999999999999999999999999999"};
  for (const char *s : invalid) assert(!print_parse_length(s,s+strlen(s),8192,&length));
  puts("ASCII filtering, CRLF/tab handling, staging, bounds and HTTP length tests passed");
}
