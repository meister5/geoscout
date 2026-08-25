#include "gnss.h"

#include <Arduino.h>

#include "../config.h"

namespace geoscout {
namespace hal {
namespace {
HardwareSerial g_serial(1);
}  // namespace

bool Gnss::begin() {
    // Before begin(), not after: the ESP32 core refuses to resize a buffer that
    // is already allocated ("RX Buffer can't be resized when Serial is already
    // running") and leaves it at the 256-byte default. The receiver emits a
    // full NMEA cycle at up to 10 Hz, which at 115200 baud is more than 256
    // bytes between two frames of a slow app -- so getting this order wrong
    // costs whole sentences rather than nothing.
    g_serial.setRxBufferSize(2048);
    g_serial.begin(kGnssBaud, SERIAL_8N1, pins::kGnssRx, pins::kGnssTx);
    return true;
}

bool Gnss::poll(uint64_t nowMs) {
    bool updated = false;
    // Bounded per call so a flood of sentences cannot starve the frame loop and
    // stall the globe.
    int budget = 512;
    while (g_serial.available() > 0 && budget-- > 0) {
        const int c = g_serial.read();
        if (c < 0) break;
        ++bytesRead_;
        parser_.encode(static_cast<char>(c));
    }
    if (parser_.takeUpdated()) {
        lastFixMs_ = nowMs;
        updated = true;
    }
    return updated;
}

uint32_t Gnss::fixAgeMs(uint64_t nowMs) const {
    if (lastFixMs_ == 0) return UINT32_MAX;
    if (nowMs <= lastFixMs_) return 0;
    const uint64_t age = nowMs - lastFixMs_;
    return age > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(age);
}

}  // namespace hal
}  // namespace geoscout
