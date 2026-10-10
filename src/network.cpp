#include "network.h"
#include "config.h"
#include "usb_serial.h"
#include "setup_form.h"
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <ArduinoMDNS.h>
#include <EEPROM.h>
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

struct __attribute__((packed)) Credentials {
  uint32_t magic, sequence;
  char ssid[33], password[65];
  uint32_t checksum;
};
static const uint32_t MAGIC = 0x55534232;
// Keep the backup in a separate physical erase block on the RA4M1.
static const int SLOTS[] = {0, FLASH_BLOCK_SIZE};
static_assert(sizeof(Credentials) <= FLASH_BLOCK_SIZE, "Credential record must fit one erase block");
static Credentials saved, candidate;
static int active_slot = -1;
enum class Mode { Off, Setup, Testing, Online, Recovering };
static Mode mode = Mode::Off;
static WiFiServer web(80);
static WiFiUDP udp;
static MDNS mdns(udp);
static bool mdns_up = false, service_added = false, test_pending = false;
static bool candidate_test = false, stable = false;
static uint32_t test_after, join_start, stable_since, retry_at;
static uint8_t retries = 0;
static char result[180] = "Choose a network and test it before saving.";
struct Network { char ssid[33]; int32_t rssi; uint8_t security; };
static Network networks[16];
static uint8_t network_count = 0;
static bool scan_done = false, scan_failed = false;
static WiFiClient request_client;
static char request[1200], body[400];
static size_t request_used = 0, body_used = 0;
static int body_length = 0;
static bool headers_done = false;
static uint32_t request_start;

static uint32_t checksum(const Credentials &c) {
  uint32_t h = 2166136261UL;
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&c);
  for (size_t i = 0; i < offsetof(Credentials, checksum); ++i) h = (h ^ p[i]) * 16777619UL;
  return h;
}
static bool valid(const Credentials &c) {
  return c.magic == MAGIC && c.checksum == checksum(c) && c.ssid[32] == 0 && c.password[64] == 0;
}
static void load() {
  Credentials records[2];
  for (int i = 0; i < 2; ++i) EEPROM.get(SLOTS[i], records[i]);
  active_slot = -1;
  for (int i = 0; i < 2; ++i)
    if (valid(records[i]) && (active_slot < 0 || static_cast<int32_t>(records[i].sequence - records[active_slot].sequence) > 0)) active_slot = i;
  memset(&saved, 0, sizeof(saved));
  if (active_slot >= 0) saved = records[active_slot];
  else {
    strncpy(saved.ssid, WIFI_SSID, 32);
    strncpy(saved.password, WIFI_PASS, 64);
  }
}
static bool commit_verified() {
  int next = active_slot == 0 ? 1 : 0;
  candidate.magic = MAGIC; candidate.sequence = saved.sequence + 1;
  candidate.checksum = checksum(candidate);
  EEPROM.put(SLOTS[next], candidate);
  Credentials check; EEPROM.get(SLOTS[next], check);
  if (!valid(check) || memcmp(&check, &candidate, sizeof(check))) return false;
  saved = candidate; active_slot = next; return true;
}
const char *network_mode() {
  switch (mode) {
    case Mode::Setup: return "setup AP";
    case Mode::Testing: return "testing credentials/DHCP";
    case Mode::Online: return "online";
    case Mode::Recovering: return "recovering saved network";
    default: return "Wi-Fi unavailable";
  }
}
const char *network_result() { return result; }
bool network_ready() {
  return mode == Mode::Online && WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0);
}
static void finish_request() {
  request_client.stop(); request_used = body_used = 0; headers_done = false;
}
static void stop_services() {
  finish_request(); web.end(); udp.stop(); mdns_up = false;
}
static void setup_ap(const char *message) {
  stop_services(); WiFi.end();
  mode = Mode::Setup; test_pending = false; stable = false;
  strncpy(result, message, sizeof(result)-1); result[sizeof(result)-1] = 0;
  uint8_t status = WiFi.beginAP(BRIDGE_SETUP_SSID);
  web.begin();
  Serial.print("Setup AP status: "); Serial.println(status);
  Serial.print("Join "); Serial.println(BRIDGE_SETUP_SSID);
  Serial.print("Open http://"); Serial.println(WiFi.localIP());
  Serial.println(result);
}
void network_open_setup() {
  retries = 0;
  setup_ap("Setup opened by CANCEL/RESET. Previous verified settings retained until a new network passes its test.");
}
static void announce() {
  web.begin();
  if (mdns.begin(WiFi.localIP(), BRIDGE_HOSTNAME) == MDNSSuccess) {
    mdns_up = true;
    if (!service_added)
      service_added = mdns.addServiceRecord("USB serial._serial", BRIDGE_TCP_PORT, MDNSServiceTCP) == MDNSSuccess;
  }
  Serial.print("Verified Wi-Fi IP: "); Serial.println(WiFi.localIP());
  Serial.println(result);
}
static void join(const Credentials &c, bool proposed) {
  stop_services(); WiFi.end();
  mode = Mode::Testing; candidate = c; candidate_test = proposed;
  stable = false; stable_since = 0;
  WiFi.setHostname(BRIDGE_HOSTNAME);
  Serial.print("Testing SSID: "); Serial.println(candidate.ssid);
  // WiFiS3 association is synchronous. Check DHCP/stability later in loop.
  // No reboot and no EEPROM mutation at this stage.
  if (candidate.password[0]) WiFi.begin(candidate.ssid, candidate.password);
  else WiFi.begin(candidate.ssid);
  join_start = millis();
}
void network_begin(bool reset) {
  load();
  if (reset) {
    // Persist an empty, checksummed tombstone so secrets.h cannot override
    // a deliberate reset on the following boot.
    memset(&candidate, 0, sizeof(candidate));
    if (!commit_verified()) {
      setup_ap("Credential reset could not be verified; previous settings retained. Retry boot reset.");
      return;
    }
    // The newest empty record overrides the older credentials. Keeping that
    // older block untouched makes the reset commit atomic like a normal save.
  }
  if (WiFi.status() == WL_NO_MODULE) { Serial.println("Wi-Fi module missing"); return; }
  if (!saved.ssid[0]) setup_ap("Choose a network. Saved only after association and DHCP succeed.");
  else join(saved, false);
}
static const char *security_name(uint8_t security) {
  switch (security) {
    case ENC_TYPE_NONE: return "Open";
    case ENC_TYPE_WPA: return "WPA personal";
    case ENC_TYPE_WPA2: return "WPA2 personal";
    case ENC_TYPE_WPA3: return "WPA3/mixed (module support varies)";
    case ENC_TYPE_WPA2_ENTERPRISE: return "Enterprise - unsupported";
    case ENC_TYPE_WEP: return "WEP - unsupported";
    default: return "Unknown security";
  }
}
static void scan() {
  int count = WiFi.scanNetworks();
  network_count = 0; scan_done = true; scan_failed = count < 0;
  for (int i = 0; i < count && network_count < 16; ++i) {
    const char *ssid = WiFi.SSID(i);
    if (!ssid || !ssid[0]) continue;
    bool duplicate = false;
    for (uint8_t j = 0; j < network_count; ++j) if (!strcmp(networks[j].ssid, ssid)) duplicate = true;
    if (duplicate) continue;
    Network &n = networks[network_count++];
    strncpy(n.ssid, ssid, 32); n.ssid[32] = 0;
    n.rssi = WiFi.RSSI(i); n.security = WiFi.encryptionType(i);
  }
}
static void html(Print &out, const char *s) {
  for (; *s; ++s) {
    switch (*s) {
      case '&': out.print("&amp;"); break;
      case '<': out.print("&lt;"); break;
      case '>': out.print("&gt;"); break;
      case '"': out.print("&quot;"); break;
      case '\'': out.print("&#39;"); break;
      default: out.write(static_cast<uint8_t>(*s));
    }
  }
}
static void respond(WiFiClient &c, const char *status, const char *type) {
  c.print("HTTP/1.1 "); c.println(status);
  c.print("Content-Type: "); c.println(type);
  c.println("Cache-Control: no-store\r\nConnection: close\r\n");
}
static void mode_page(WiFiClient &c) {
  respond(c, "200 OK", "text/html; charset=utf-8");
  c.println("<!doctype html><meta name=viewport content='width=device-width'><title>USB mode</title><style>body{font:17px system-ui;max-width:650px;margin:30px auto;padding:16px}button,select{font:inherit;padding:9px}</style>");
  c.println("<h2>USB mode</h2><p>Current driver: <b>"); html(c, usb_driver()); c.println("</b></p>");
  if (usb_driver_mode_switch_supported()) {
    c.print("<form method=post action=/mode><select name=mode>");
    c.print("<option value=acm"); if (!strcmp(usb_driver(), "CDC-ACM")) c.print(" selected");
    c.print(">CDC-ACM</option><option value=printer"); if (!strcmp(usb_driver(), "USB Printer Class")) c.print(" selected");
    c.println(">USB Printer Class</option></select> <button>Switch and reboot</button></form>");
    c.println("<p><small>Changing this setting interrupts the current print session and reboots the bridge. Wi-Fi credentials are retained.</small></p>");
  } else c.println("<p><small>USB mode switching requires the unified firmware image.</small></p>");
  c.println("<p><a href=/status>Status</a></p>");
}
static void page(WiFiClient &c) {
  respond(c, "200 OK", "text/html; charset=utf-8");
  c.println("<!doctype html><meta name=viewport content='width=device-width'><title>USB Serial Wi-Fi</title><style>body{font:17px system-ui;max-width:650px;margin:30px auto;padding:16px}input,button,select{font:inherit;padding:9px;max-width:95%}table{width:100%;text-align:left}td{padding:8px}small{color:#555}</style><h1>USB Serial Wi-Fi</h1>");
  c.print("<p><b>"); html(c, network_mode()); c.print("</b></p><p>"); html(c, result); c.println("</p><p><a href=/status>Status</a></p>");
  if (mode != Mode::Setup) {
    c.println("<p>Hold CANCEL/RESET for 3 seconds to stop the stream and open setup. Scanning/changing Wi-Fi is available there.</p>"); return;
  }
  c.println("<form method=post action=/scan><button>Scan / refresh networks</button></form>");
  if (scan_failed) c.println("<p>Scan failed. Retry or enter the network name manually.</p>");
  else if (scan_done && !network_count) c.println("<p>No named networks found. Check range/band or enter a hidden SSID.</p>");
  c.println("<form method=post action=/test><p>Network name (select or type)<br><input name=ssid list=networks maxlength=32 required autocomplete=off></p><datalist id=networks>");
  for (uint8_t i = 0; i < network_count; ++i) {
    c.print("<option value=\""); html(c, networks[i].ssid); c.print("\">");
    c.print(networks[i].rssi); c.print(" dBm - "); html(c, security_name(networks[i].security)); c.println("</option>");
  }
  c.println("</datalist><p>Password (blank for an open network)<br><input type=password name=password maxlength=64 autocomplete=new-password></p><button>Test connection and save if successful</button></form>");
  c.println("<p><small>Testing temporarily disconnects this setup Wi-Fi. Success: join the selected network and open http://usbserial.local/status. Failure: rejoin USB-Serial-Setup and reload this page for the result. Previous verified settings remain intact. No reboot is required. Personal/open networks only; captive-portal login is not handled.</small></p>");
  if (network_count) {
    c.println("<h2>Nearby networks</h2><table><tr><th>Name</th><th>Signal</th><th>Security</th></tr>");
    for (uint8_t i = 0; i < network_count; ++i) {
      c.print("<tr><td>"); html(c, networks[i].ssid); c.print("</td><td>"); c.print(networks[i].rssi);
      c.print(" dBm</td><td>"); html(c, security_name(networks[i].security)); c.println("</td></tr>");
    }
    c.println("</table>");
  }
}
static void bad_request(const char *reason) {
  respond(request_client, "400 Bad Request", "text/plain"); request_client.println(reason); finish_request();
}
static bool equal_ci(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
    if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
    if (x != y) return false;
  }
  return true;
}
static bool parse_headers() {
  body_length = 0; bool have_length = false;
  const char *p = strstr(request, "\r\n");
  if (!p) return false;
  p += 2;
  while (*p && strncmp(p, "\r\n", 2)) {
    const char *end = strstr(p, "\r\n");
    if (!end) return false;
    size_t n = end-p;
    if (n >= 15 && equal_ci(p, "Content-Length:", 15)) {
      if (have_length) return false;
      have_length = true; const char *v = p+15;
      while (v < end && (*v == ' ' || *v == '\t')) ++v;
      if (v == end) return false;
      while (v < end) {
        if (*v < '0' || *v > '9') return false;
        body_length = body_length*10 + (*v++ - '0');
        if (body_length >= static_cast<int>(sizeof(body))) return false;
      }
    } else if (n >= 18 && equal_ci(p, "Transfer-Encoding:", 18)) return false;
    p = end+2;
  }
  return strncmp(request, "POST ", 5) || have_length;
}
static void process_request() {
  body[body_used] = 0;
  bool reboot = false;
  if (!strncmp(request, "GET /status ", 12)) {
    respond(request_client, "200 OK", "text/plain"); bridge_status(request_client);
  } else if (!strncmp(request, "GET /mode ", 10)) {
    mode_page(request_client);
  } else if (!strncmp(request, "POST /mode ", 11)) {
    char selected[12] = {};
    if (!usb_driver_mode_switch_supported() || !form_field(body, "mode", selected, sizeof(selected)) ||
        !usb_set_driver_mode(selected)) {
      bad_request("Unsupported USB mode"); return;
    }
    respond(request_client, "200 OK", "text/html; charset=utf-8");
    request_client.println("<!doctype html><meta name=viewport content='width=device-width'><h1>USB mode saved</h1><p>Rebooting into the selected driver. Reload the page after the bridge reconnects.</p>");
    reboot = true;
  } else if (!strncmp(request, "POST /scan ", 11)) {
    if (mode != Mode::Setup || test_pending) { bad_request("Open setup with the button first"); return; }
    scan(); page(request_client);
  } else if (!strncmp(request, "POST /test ", 11)) {
    if (mode != Mode::Setup || test_pending) { bad_request("Open setup with the button first"); return; }
    Credentials proposed = {};
    if (!form_field(body, "ssid", proposed.ssid, sizeof(proposed.ssid)) || !proposed.ssid[0] ||
        !form_field(body, "password", proposed.password, sizeof(proposed.password)) || !personal_password_valid(proposed.password)) {
      bad_request("Invalid SSID/password. SSID: 1-32 bytes. Password: blank, 8-63 characters or 64 hex digits."); return;
    }
    for (uint8_t i = 0; i < network_count; ++i) if (!strcmp(networks[i].ssid, proposed.ssid)) {
      if (networks[i].security == ENC_TYPE_WPA2_ENTERPRISE || networks[i].security == ENC_TYPE_WEP) {
        bad_request("Network needs an unsupported authentication method."); return;
      }
      if (networks[i].security != ENC_TYPE_NONE && !proposed.password[0]) {
        bad_request("Password required for this scanned network."); return;
      }
    }
    candidate = proposed; test_pending = true; test_after = millis();
    respond(request_client, "200 OK", "text/html; charset=utf-8");
    request_client.println("<!doctype html><meta name=viewport content='width=device-width'><h1>Testing your network</h1><p>The setup Wi-Fi will disconnect temporarily. No reboot and no settings are saved yet.</p><p>Allow up to 45 seconds. Success: join the selected network and open <a href=http://usbserial.local/status>usbserial.local/status</a>. Failure: rejoin USB-Serial-Setup and reopen its setup address to see the result. The USB-C console also reports the result.</p>");
  } else page(request_client);
  finish_request();
  if (reboot) { delay(250); NVIC_SystemReset(); }
}
static void web_poll() {
  if (!request_client) {
    request_client = web.accept();
    if (!request_client) return;
    request_used = body_used = 0; headers_done = false; request_start = millis();
  }
  if (millis()-request_start > 5000) { bad_request("Request timed out; nothing saved"); return; }
  size_t budget = 128;
  while (budget-- && request_client.available()) {
    char ch = static_cast<char>(request_client.read());
    if (!headers_done) {
      if (!ch || request_used >= sizeof(request)-1) { bad_request("Invalid/oversized headers"); return; }
      request[request_used++] = ch; request[request_used] = 0;
      if (request_used >= 4 && !strcmp(request+request_used-4, "\r\n\r\n")) {
        if (!parse_headers()) { bad_request("Invalid HTTP framing; nothing saved"); return; }
        headers_done = true;
        if (!body_length) { process_request(); return; }
      }
    } else {
      if (!ch) { bad_request("NUL in form; nothing saved"); return; }
      body[body_used++] = ch;
      if (body_used == static_cast<size_t>(body_length)) { process_request(); return; }
    }
  }
  if (!request_client.connected() && !request_client.available()) finish_request();
}
void network_poll() {
  if (mode == Mode::Setup || mode == Mode::Online) web_poll();
  if (mode == Mode::Setup && test_pending && millis()-test_after >= 500) {
    test_pending = false;
    Credentials proposed = candidate; join(proposed, true);
  }
  if (mode == Mode::Testing) {
    bool associated = WiFi.status() == WL_CONNECTED;
    bool ip = WiFi.localIP() != IPAddress(0,0,0,0);
    bool matching = associated && !strcmp(WiFi.SSID(), candidate.ssid);
    if (associated && ip && matching) {
      if (!stable) { stable = true; stable_since = millis(); }
      if (valid_saved_network(associated, ip, matching, millis()-stable_since)) {
        // Initial compile-time secrets are committed after verification too.
        // Reconnecting an existing saved network does not wear EEPROM.
        if ((candidate_test || active_slot < 0) && !commit_verified()) {
          setup_ap("Connected, but EEPROM verification failed. Previous verified settings retained. Please retry."); return;
        }
        mode = Mode::Online; retries = 0;
        strncpy(result, "Connection verified: association/authentication and DHCP succeeded. Settings saved; no reboot needed.", sizeof(result));
        announce();
      }
    } else stable = false;
    if (mode == Mode::Testing && millis()-join_start >= 15000) {
      if (candidate_test || !saved.ssid[0] || retries >= 3)
        setup_ap("Connection test failed: association/authentication or DHCP did not complete. Previous verified settings retained. Check password, security and DHCP, then retry.");
      else { mode = Mode::Recovering; retry_at = millis(); }
    }
  } else if (mode == Mode::Online) {
    if (!network_ready()) {
      stop_services(); mode = Mode::Recovering; retries = 0; retry_at = millis();
      strncpy(result, "Wi-Fi lost. Retrying the last verified network before opening setup.", sizeof(result));
    } else if (mdns_up) mdns.run();
  } else if (mode == Mode::Recovering && millis()-retry_at >= (2000UL << retries)) {
    if (retries >= 3) setup_ap("Saved network could not be recovered. Settings retained. Test a new network or power cycle to retry.");
    else { ++retries; join(saved, false); }
  }
}
