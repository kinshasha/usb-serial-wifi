#include "byte_queue.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
int main() {
  ByteQueue<4> q;
  uint8_t data[8];
  assert(q.push(0) && q.push(0xff) && q.push(2));
  assert(q.peek(data, 8) == 3 && data[0] == 0 && data[1] == 0xff);
  q.drop(2);
  assert(q.push(3) && q.push(4) && q.push(5) && !q.push(6));
  assert(q.peek(data, 8) == 4 && data[0] == 2 && data[3] == 5);
  q.drop(99); assert(!q.size());
  LineEnding line;
  assert(line.append(q, '\r', true) && line.append(q, '\n', true));
  assert(q.size() == 2);
  q.clear(); line.reset();
  assert(q.push('x') && q.push('y') && q.push('z'));
  assert(!line.append(q, '\n', true) && q.size() == 3);
  q.drop(2);
  assert(line.append(q, '\n', true));
  assert(q.peek(data, 8) == 3 && data[1] == '\r' && data[2] == '\n');
  q.clear(); line.reset();
  for (uint8_t b : {uint8_t(0), uint8_t(0x1b), uint8_t(0x0c), uint8_t(0xff)})
    assert(line.append(q, b, false));
  assert(q.peek(data, 8) == 4 && data[0] == 0 && data[1] == 0x1b && data[2] == 0x0c && data[3] == 0xff);
  puts("Queue and byte-preservation tests passed");
}
