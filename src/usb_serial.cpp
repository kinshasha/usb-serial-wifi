#include "config.h"
#include "usb_serial.h"
#include <SPI.h>
#include <cdcacm.h>
#include <EEPROM.h>
#if BRIDGE_DRIVER == 5
#include <USBPrinter.h>
#elif BRIDGE_DRIVER == 2
#include <cdcftdi.h>
#elif BRIDGE_DRIVER == 3
#include <cdcprolific.h>
#elif BRIDGE_DRIVER == 4
#include <USBPrinter.h>
#endif

static USB host;
static bool host_ok = false;

#if BRIDGE_DRIVER == 5
static bool printer_mode = false;
class ConfigureAcm : public CDCAsyncOper {
  uint8_t OnInit(ACM *device) override {
    LINE_CODING coding = {};
    coding.dwDTERate = BRIDGE_BAUD;
    coding.bCharFormat = BRIDGE_STOP_BITS;
    coding.bParityType = BRIDGE_PARITY;
    coding.bDataBits = BRIDGE_DATA_BITS;
    uint8_t rc = device->SetLineCoding(&coding);
    if (rc) return rc;
    return device->SetControlLineState(BRIDGE_CONTROL_LINES);
  }
};
class ConfigurePrinter : public USBPrinterAsyncOper {};
static ConfigureAcm acm_configure;
static ConfigurePrinter printer_configure;
static ACM acm_device(&host, &acm_configure);
static USBPrinter printer_device(&host, &printer_configure);
const char *usb_driver() { return printer_mode ? "USB Printer Class" : "CDC-ACM"; }
#elif BRIDGE_DRIVER == 4
class ConfigurePrinter : public USBPrinterAsyncOper {};
static ConfigurePrinter configure;
static USBPrinter device(&host, &configure);
const char *usb_driver() { return "USB Printer Class"; }
#elif BRIDGE_DRIVER == 2
class ConfigureSerial : public FTDIAsyncOper {
  uint8_t OnInit(FTDI *device) override {
    uint8_t rc = device->SetBaudRate(BRIDGE_BAUD);
    if (rc) return rc;
    // FTDI stop-bit encoding differs from CDC's 1.5/2 ordering.
    const uint16_t stop = BRIDGE_STOP_BITS == 1 ? FTDI_SIO_SET_DATA_STOP_BITS_15 :
                          BRIDGE_STOP_BITS == 2 ? FTDI_SIO_SET_DATA_STOP_BITS_2 : 0;
    rc = device->SetData(BRIDGE_DATA_BITS | (BRIDGE_PARITY << 8) | stop);
    if (rc) return rc;
    rc = device->SetModemControl(
      ((BRIDGE_CONTROL_LINES & 1) ? FTDI_SIO_SET_DTR_HIGH : FTDI_SIO_SET_DTR_LOW) |
      ((BRIDGE_CONTROL_LINES & 2) ? FTDI_SIO_SET_RTS_HIGH : FTDI_SIO_SET_RTS_LOW));
    if (rc) return rc;
    return device->SetFlowControl(FTDI_SIO_DISABLE_FLOW_CTRL);
  }
};
static ConfigureSerial configure;
static FTDI device(&host, &configure);
const char *usb_driver() { return "FTDI"; }
#elif BRIDGE_DRIVER == 1 || BRIDGE_DRIVER == 3
class ConfigureSerial : public CDCAsyncOper {
  uint8_t OnInit(ACM *device) override {
    LINE_CODING coding = {};
    coding.dwDTERate = BRIDGE_BAUD;
    coding.bCharFormat = BRIDGE_STOP_BITS;
    coding.bParityType = BRIDGE_PARITY;
    coding.bDataBits = BRIDGE_DATA_BITS;
    uint8_t rc = device->SetLineCoding(&coding);
    if (rc) return rc;
    return device->SetControlLineState(BRIDGE_CONTROL_LINES);
  }
};
static ConfigureSerial configure;
#if BRIDGE_DRIVER == 3
static PL2303 device(&host, &configure);
const char *usb_driver() { return "PL2303"; }
#else
static ACM device(&host, &configure);
const char *usb_driver() { return "CDC-ACM"; }
#endif
#else
#error "Select a supported BRIDGE_DRIVER"
#endif

bool usb_driver_mode_switch_supported() { return BRIDGE_DRIVER == 5; }
bool usb_set_driver_mode(const char *mode) {
#if BRIDGE_DRIVER == 5
  if (!mode || (strcmp(mode, "acm") && strcmp(mode, "printer"))) return false;
  EEPROM.update(BRIDGE_DRIVER_EEPROM_ADDR, !strcmp(mode, "printer") ? 1 : 0);
  return true;
#else
  (void)mode;
  return false;
#endif
}

bool usb_begin() {
#if BRIDGE_DRIVER == 5
  printer_mode = EEPROM.read(BRIDGE_DRIVER_EEPROM_ADDR) == 1;
#endif
  const int8_t rc = host.Init();
  Serial.print("Selected USB driver: "); Serial.println(usb_driver());
  Serial.print("MAX3421E Init rc="); Serial.println(rc);
  host_ok = rc == 0; return host_ok;
}
void usb_poll() {
  if (!host_ok) return;
  host.Task();
  static uint8_t previous_state = 0xff;
  const uint8_t state = host.getUsbTaskState();
  if (state != previous_state) {
    Serial.print("USB task state=0x"); Serial.println(state, HEX);
    previous_state = state;
  }
#if BRIDGE_DRIVER == 4 || BRIDGE_DRIVER == 5
#if BRIDGE_DRIVER == 5
  if (!printer_mode) return;
  USBPrinter &active_printer = printer_device;
#else
  USBPrinter &active_printer = device;
#endif
  static uint32_t last_poll = 0;
  static uint16_t previous_status = 0xffff;
  if (!active_printer.isReady()) { previous_status = 0xffff; return; }
  if (millis() - last_poll < 1000) return;
  last_poll = millis();
  uint8_t status = 0;
  const uint8_t rc = active_printer.ReadPortStatus(&status);
  const uint16_t observation = (uint16_t(rc) << 8) | status;
  if (observation != previous_status) {
    Serial.print("GET_PORT_STATUS rc=0x"); Serial.print(rc, HEX);
    if (!rc) {
      Serial.print(" status=0x"); Serial.print(status, HEX);
      Serial.print(" paper_empty="); Serial.print((status & 0x20) != 0);
      Serial.print(" selected="); Serial.print((status & 0x10) != 0);
      Serial.print(" not_error="); Serial.print((status & 0x08) != 0);
    }
    Serial.println(); previous_status = observation;
  }
#endif
}
bool usb_ready() {
#if BRIDGE_DRIVER == 5
  return host_ok && (printer_mode ? printer_device.isReady() : acm_device.isReady());
#else
  return host_ok && device.isReady();
#endif
}
uint8_t usb_state() { return host.getUsbTaskState(); }
uint16_t usb_tx_packet_size() {
#if BRIDGE_DRIVER == 5
  if (!printer_mode) return 1;
  const uint16_t packet = printer_device.epInfo[USBPrinter::epDataOutIndex].maxPktSize;
#elif BRIDGE_DRIVER == 4
  const uint16_t packet = device.epInfo[USBPrinter::epDataOutIndex].maxPktSize;
#else
  return 1;
#endif
#if BRIDGE_DRIVER == 4 || BRIDGE_DRIVER == 5
  return packet && packet <= 64 ? packet : 1;
#endif
}
uint8_t usb_send(uint16_t count, uint8_t *data) {
#if BRIDGE_DRIVER == 5
  return printer_mode ? printer_device.SndData(count, data) : acm_device.SndData(count, data);
#else
  return device.SndData(count, data);
#endif
}
uint8_t usb_receive(uint16_t *count, uint8_t *data) {
#if BRIDGE_DRIVER == 4
  // Raw print-only interface: no serial return channel in this first build.
  (void)data; *count = 0; return 0;
#elif BRIDGE_DRIVER == 5
  if (printer_mode) { (void)data; *count = 0; return 0; }
  return acm_device.RcvData(count, data);
#else
  uint8_t rc = device.RcvData(count, data);
  if (rc) { *count = 0; return rc; }
#if BRIDGE_DRIVER == 2
  // We request at most one 64-byte USB packet. FTDI adds two status bytes
  // to EACH packet; never forward those as printer/serial payload.
  if (*count <= 2) *count = 0;
  else { *count -= 2; memmove(data, data + 2, *count); }
#endif
  return rc;
#endif
}
