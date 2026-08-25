// The one place colours are decided.
//
// Values are RGB565 literals rather than calls into the display library so that
// they can sit in constant expressions and so the palette can be read at a
// glance. The comment on each is the RGB888 it was derived from.
#pragma once

#include <cstdint>

namespace geoscout {
namespace theme {

// --- chrome ---------------------------------------------------------------
constexpr uint16_t kBackground = 0x0041;   // 6, 8, 14      near-black, slightly blue
constexpr uint16_t kPanel      = 0x18E4;   // 24, 28, 38    status bar, list rows
constexpr uint16_t kPanelHi    = 0x2965;   // 40, 44, 44    selected list row
constexpr uint16_t kText       = 0xE75E;   // 230, 235, 240
constexpr uint16_t kTextDim    = 0x8472;   // 130, 140, 150
constexpr uint16_t kTextFaint  = 0x4A49;   // 72, 76, 76
constexpr uint16_t kAccent     = 0x06FF;   // 0, 220, 255   cyan
constexpr uint16_t kGood       = 0x3ECF;   // 60, 220, 120  green
constexpr uint16_t kWarn       = 0xFDA5;   // 255, 180, 40  amber
constexpr uint16_t kBad        = 0xFA8A;   // 255, 80, 80   red

// --- globe ----------------------------------------------------------------
// The lit and unlit pairs are what make the terminator legible: each outline
// segment picks one or the other from the sign of its dot product with the
// subsolar direction, so day and night differ in brightness rather than in
// anything that needs a fill.
constexpr uint16_t kOceanDay   = 0x0A6B;   // 8, 76, 92     lit sea
constexpr uint16_t kOceanNight = 0x0124;   // 0, 36, 36     unlit sea
constexpr uint16_t kCoastDay   = 0xAF7F;   // 168, 238, 255
constexpr uint16_t kCoastNight = 0x2C51;   // 40, 138, 138
constexpr uint16_t kBorderDay  = 0x5B37;   // 88, 102, 184
constexpr uint16_t kBorderNight= 0x18C7;   // 24, 24, 60
constexpr uint16_t kLakeDay    = 0x4D5F;   // 72, 172, 255
constexpr uint16_t kLakeNight  = 0x10AA;   // 16, 20, 84
constexpr uint16_t kGraticule  = 0x2124;   // 32, 36, 36
constexpr uint16_t kStar       = 0x4208;   // 64, 64, 64
constexpr uint16_t kStarBright = 0x8410;   // 128, 132, 128
constexpr uint16_t kSun        = 0xFF00;   // 255, 228, 0
constexpr uint16_t kHere       = 0xF9E7;   // 248, 60, 60

// --- sky view -------------------------------------------------------------
// One colour per constellation, chosen to stay distinguishable at three pixels
// across. See constellationColor() in the Sky View app.
constexpr uint16_t kGps        = 0x06FF;   // cyan
constexpr uint16_t kGlonass    = 0xFDA5;   // amber
constexpr uint16_t kGalileo    = 0x3ECF;   // green
constexpr uint16_t kBeidou     = 0xF81F;   // magenta
constexpr uint16_t kQzss       = 0x849F;   // periwinkle
constexpr uint16_t kOtherSat   = 0x8472;   // grey

}  // namespace theme
}  // namespace geoscout
