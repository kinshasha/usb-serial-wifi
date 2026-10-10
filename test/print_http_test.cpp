#include "print_http.h"
#include <assert.h>
#include <stdio.h>
#include <string>
int main() {
  int length=-1;
  const char *good[] = {
    "POST /print HTTP/1.1\r\nContent-Length: 8192\r\nContent-Type: application/octet-stream\r\n\r\n",
    "POST /print HTTP/1.1\r\ncontent-length: 5\r\ncontent-type: text/plain; charset=utf-8\r\n\r\n",
    "POST /test HTTP/1.1\r\nContent-Length: 399\r\n\r\n",
    "GET /print HTTP/1.1\r\n\r\n"
  };
  for(const char *s:good) assert(print_request_headers(s,399,8192,&length));
  const char *bad[] = {
    "POST /print HTTP/1.1\r\nContent-Length: 8193\r\nContent-Type: text/plain\r\n\r\n",
    "POST /print HTTP/1.1\r\nContent-Length: 0\r\nContent-Type: text/plain\r\n\r\n",
    "POST /print HTTP/1.1\r\nContent-Length: 1\r\n\r\n",
    "POST /print HTTP/1.1\r\nContent-Length: 1\r\nContent-Type: multipart/form-data; boundary=x\r\n\r\n",
    "POST /print HTTP/1.1\r\nContent-Length: 1\r\nContent-Type: text/plain\r\nContent-Type: text/plain\r\n\r\n",
    "POST /print HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 2\r\nContent-Type: text/plain\r\n\r\n",
    "POST /print HTTP/1.1\r\nContent-Length: 9999999999999999999999999\r\nContent-Type: text/plain\r\n\r\n",
    "POST /print HTTP/1.1\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n",
    "POST /test HTTP/1.1\r\nContent-Length: 400\r\n\r\n"
  };
  for(const char *s:bad) assert(!print_request_headers(s,399,8192,&length));
  puts("HTTP upload length, media type, duplicate headers and framing tests passed");
}
