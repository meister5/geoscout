// ATGM336H on UART1. Reads NMEA and feeds the host-tested parser in lib/core.
#pragma once

#include <cstdint>

#include "nmea.h"

namespace geoscout {
namespace hal {

class Gnss {
public:
    bool begin();

    // Drains the UART. Returns true when a sentence updated the position.
    bool poll(uint64_t nowMs);

    const GnssFix& fix() const { return parser_.fix(); }
    const NmeaParser& parser() const { return parser_; }

    // Milliseconds since the last position update, which is what tells a
    // stationary reading from a stale one. The status bar shows it for exactly
    // that reason.
    uint32_t fixAgeMs(uint64_t nowMs) const;
    bool everHadFix() const { return lastFixMs_ != 0; }

    uint32_t bytesRead() const { return bytesRead_; }

private:
    NmeaParser parser_;
    uint64_t lastFixMs_ = 0;
    uint32_t bytesRead_ = 0;
};

}  // namespace hal
}  // namespace geoscout
