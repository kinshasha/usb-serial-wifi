Import("env")

from pathlib import Path


def patch_usb_host(source, target, env):
    candidates = [
        Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "USB-Host-Shield-20" / "Usb.cpp",
    ]
    path = next((candidate for candidate in candidates if candidate.exists()), None)
    if path is None:
        raise RuntimeError("USB Host Shield source was not found")

    text = path.read_text()
    marker = "// Codex timeout guard for UNO R4 USB transfers"
    if marker in text:
        return

    old_first = """                while(!(regRd(rHIRQ) & bmHXFRDNIRQ)){
#if defined(ESP8266) || defined(ESP32)
                        yield(); // needed in order to reset the watchdog timer on the ESP8266
#endif
                } //wait for the completion IRQ"""
    new_first = """                // Codex timeout guard for UNO R4 USB transfers
                uint32_t irq_timeout = (uint32_t)millis() + USB_XFER_TIMEOUT;
                while(!(regRd(rHIRQ) & bmHXFRDNIRQ)){
#if defined(ESP8266) || defined(ESP32)
                        yield(); // needed in order to reset the watchdog timer on the ESP8266
#endif
                        if ((int32_t)((uint32_t)millis() - irq_timeout) >= 0L) {
                                rcode = USB_ERROR_TRANSFER_TIMEOUT;
                                goto breakout;
                        }
                } //wait for the completion IRQ"""
    old_second = """                        while(!(regRd(rHIRQ) & bmHXFRDNIRQ)){
#if defined(ESP8266) || defined(ESP32)
                        yield(); // needed in order to reset the watchdog timer on the ESP8266
#endif
                        } //wait for the completion IRQ"""
    new_second = """                        // Codex timeout guard for UNO R4 USB transfers
                        uint32_t irq_timeout_retry = (uint32_t)millis() + USB_XFER_TIMEOUT;
                        while(!(regRd(rHIRQ) & bmHXFRDNIRQ)){
#if defined(ESP8266) || defined(ESP32)
                        yield(); // needed in order to reset the watchdog timer on the ESP8266
#endif
                        if ((int32_t)((uint32_t)millis() - irq_timeout_retry) >= 0L) {
                                rcode = USB_ERROR_TRANSFER_TIMEOUT;
                                goto breakout;
                        }
                        } //wait for the completion IRQ"""
    if text.count(old_first) != 1 or text.count(old_second) != 1:
        raise RuntimeError("Unexpected USB Host Shield transfer loop layout")
    path.write_text(text.replace(old_first, new_first).replace(old_second, new_second))


patch_usb_host(None, None, env)
