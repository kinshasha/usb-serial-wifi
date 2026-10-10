#include <assert.h>
#include <stdio.h>
#include <vector>
#include <string>
#include "../src/main.cpp"
static bool fake_usb_ready = true, fake_network_ready = true;
static uint8_t fake_send_error = 0;
static std::vector<uint8_t> sent;
static const char *fake_driver = "USB Printer Class";
bool usb_begin() { return true; }
void usb_poll() {}
bool usb_ready() { return fake_usb_ready; }
const char *usb_driver() { return fake_driver; }
bool usb_driver_mode_switch_supported() { return true; }
bool usb_set_driver_mode(const char *) { return true; }
uint8_t usb_state() { return 0x90; }
uint16_t usb_tx_packet_size() { return 64; }
uint8_t usb_send(uint16_t n, uint8_t *b) {
  assert(n <= 64);
  if (fake_send_error) return fake_send_error;
  sent.insert(sent.end(), b, b+n); return 0;
}
uint8_t usb_receive(uint16_t *n, uint8_t *) { *n=0; return 0; }
void network_begin(bool) {}
void network_poll() {}
bool network_ready() { return fake_network_ready; }
void network_open_setup() {}
const char *network_mode() { return "online"; }
const char *network_result() { return "ready"; }
static void clear() {
  abort_session("test reset"); sent.clear(); fake_send_error=0;
  fake_usb_ready=fake_network_ready=true; fake_driver="USB Printer Class";
}
static void upload(const std::string &text) {
  assert(!bridge_web_begin());
  for (unsigned char b : text) assert(bridge_web_append(b));
}
int main() {
  size_t bytes=0, removed=0;
  clear(); upload("hello\n");
  for (int i=0;i<10;++i) bridge_poll();
  assert(sent.empty()); // Receiving never prints.
  assert(bridge_web_begin()); // Cannot interleave another job.
  assert(bridge_web_commit(&bytes,&removed) && bytes==7 && removed==0);
  bridge_web_abort_upload(); // Closing the completed HTTP request preserves job.
  for(int i=0;i<10;++i) bridge_poll();
  assert(std::string(sent.begin(),sent.end())=="hello\r\n" && !session);
  clear(); upload("discard me"); bridge_web_abort_upload(); bridge_poll();
  assert(sent.empty() && !session);
  clear(); upload(std::string(5000,'x'));
  assert(bridge_web_commit(&bytes,&removed));
  for(int i=0;i<200;++i) bridge_poll();
  assert(sent.size()==5002 && sent[5000]=='\r' && sent[5001]=='\n' && !session);
  clear(); upload("one packet"); assert(bridge_web_commit(&bytes,&removed));
  fake_send_error=hrNAK; bridge_poll(); assert(sent.empty() && session && tx.size());
  fake_send_error=0; bridge_poll(); assert(sent.size()==12 && !session);
  clear(); upload(std::string(200,'x')); assert(bridge_web_commit(&bytes,&removed));
  fake_send_error=0xff; bridge_poll();
  assert(sent.empty() && !session && !tx.size() && !web_job);
  clear(); upload(std::string(200,'x')); assert(bridge_web_commit(&bytes,&removed));
  bridge_poll(); assert(sent.size()==64); abort_session("cancel");
  for(int i=0;i<10;++i) bridge_poll(); assert(sent.size()==64);
  clear(); fake_usb_ready=false; assert(bridge_web_begin());
  fake_usb_ready=true; fake_driver="CDC-ACM"; assert(bridge_web_begin());
  fake_driver="USB Printer Class"; fake_network_ready=false; assert(bridge_web_begin());
  clear(); upload(std::string(8192,'x')); assert(!bridge_web_commit(&bytes,&removed));
  bridge_web_abort_upload(); bridge_poll(); assert(sent.empty() && !session);
  clear(); upload("printer removed"); fake_usb_ready=false; bridge_poll();
  assert(!session && !bridge_web_commit(&bytes,&removed) && sent.empty());
  puts("Web job staging, ownership, queue draining, NAK, cancellation and USB error tests passed");
}
