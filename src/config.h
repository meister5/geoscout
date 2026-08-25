// Build-time configuration and the Cap-Bus pin map.
#pragma once

#include <Arduino.h>

// This firmware targets the Cardputer ADV specifically. The GNSS receiver lives
// on the Cap LoRa-1262, which plugs into the rear 2x7 Cap-Bus header; the
// original Cardputer v1.1 does not have that header, and its keyboard and I2C
// layout differ besides. Building for anything else produces a binary that
// cannot work.
#if !defined(ARDUINO_ARCH_ESP32)
#error "geoscout targets the ESP32-S3 Cardputer ADV. Select the cardputer-adv PlatformIO environment."
#endif

namespace geoscout {

constexpr char kAppName[] = "GEOSCOUT";
constexpr char kAppVersion[] = "1.0.0";

namespace pins {

// GNSS UART, named from the Cardputer's point of view: the host receives the
// receiver's NMEA on kGnssRx.
constexpr int kGnssTx = 13;    // -> GPS RX
constexpr int kGnssRx = 15;    // <- GPS TX

// Shared with the HY2.0-4P Grove port.
constexpr int kI2cSda = 8;
constexpr int kI2cScl = 9;

}  // namespace pins

constexpr uint32_t kGnssBaud = 115200;

// Display geometry of the Cardputer ADV. Identical to the original Cardputer:
// the ADV changed the audio codec and the keyboard controller, not the panel.
constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;

// The strip the shell reserves at the top of the screen for fix quality,
// satellite count, battery and UTC. Apps draw under it.
constexpr int kStatusBarHeight = 12;

// NVS namespace. Settings live here and nowhere else -- this firmware creates
// nothing on the microSD card.
constexpr char kPrefsNamespace[] = "geoscout";

}  // namespace geoscout
