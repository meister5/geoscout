// Persistent settings.
//
// These live in NVS and nowhere else. geoscout does not create a directory, a
// config file or a cache on the microSD card, and nothing here should ever
// start.
#pragma once

#include <cstdint>

#include "geo.h"

namespace geoscout {

enum class Units : uint8_t {
    Metric = 0,     // km, m, km/h
    Imperial,       // mi, ft, mph
    Nautical,       // NM, ft, knots
    Count,
};

const char* unitsName(Units u);

struct Settings {
    CoordFormat coordFormat = CoordFormat::DecimalDegrees;
    Units units = Units::Metric;

    // Quarter hours east of UTC, so that Kathmandu and Chatham Island are
    // expressible rather than approximated. -48 to +56 covers every real zone.
    int8_t tzQuarterHours = 0;

    uint8_t brightness = 160;      // 16..255; the panel is unreadable below 16
    bool showStatusBar = true;

    // Globe defaults. Each is a toggle the globe also exposes on a key, so
    // these are the state it starts in rather than a lock.
    bool globeNight = true;
    bool globeGraticule = true;
    bool globeBorders = true;
    bool globeLakes = true;

    void load();
    void save();
    void restoreDefaults();

    // Minutes east of UTC, for the clock.
    int32_t tzOffsetMinutes() const { return static_cast<int32_t>(tzQuarterHours) * 15; }

    // Unit conversions, so that no app has to know what the setting means.
    double distanceFromMeters(double meters) const;   // to the large unit
    const char* distanceUnit() const;                 // "km", "mi", "NM"
    double altitudeFromMeters(double meters) const;
    const char* altitudeUnit() const;                 // "m", "ft"
    double speedFromKph(double kph) const;
    const char* speedUnit() const;                    // "km/h", "mph", "kn"
};

}  // namespace geoscout
