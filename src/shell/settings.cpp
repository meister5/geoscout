#include "settings.h"

#include <Preferences.h>

#include "../config.h"

namespace geoscout {
namespace {

Preferences g_prefs;

// One key per field rather than a blob: a blob would have to be versioned, and
// a field added later would either invalidate everyone's settings or need a
// migration. Keys are under fifteen characters, which is the NVS limit.
constexpr char kKeyFormat[] = "coordFmt";
constexpr char kKeyUnits[] = "units";
constexpr char kKeyTz[] = "tzQuarter";
constexpr char kKeyBright[] = "bright";
constexpr char kKeyStatusBar[] = "statusBar";
constexpr char kKeyNight[] = "globeNight";
constexpr char kKeyGraticule[] = "globeGrat";
constexpr char kKeyBorders[] = "globeBord";
constexpr char kKeyLakes[] = "globeLake";

template <typename T>
T clampEnum(uint8_t raw, T count, T fallback) {
    return raw < static_cast<uint8_t>(count) ? static_cast<T>(raw) : fallback;
}

}  // namespace

const char* unitsName(Units u) {
    switch (u) {
        case Units::Metric:   return "Metric";
        case Units::Imperial: return "Imperial";
        case Units::Nautical: return "Nautical";
        default:              return "?";
    }
}

void Settings::load() {
    const Settings defaults;
    if (!g_prefs.begin(kPrefsNamespace, /*readOnly=*/true)) {
        // Nothing stored yet, which is the normal state on a fresh flash.
        *this = defaults;
        return;
    }

    coordFormat = clampEnum(g_prefs.getUChar(kKeyFormat, 0),
                            CoordFormat::Count, CoordFormat::DecimalDegrees);
    units = clampEnum(g_prefs.getUChar(kKeyUnits, 0), Units::Count, Units::Metric);

    const int8_t tz = static_cast<int8_t>(g_prefs.getChar(kKeyTz, 0));
    tzQuarterHours = (tz >= -48 && tz <= 56) ? tz : 0;

    const uint8_t b = g_prefs.getUChar(kKeyBright, defaults.brightness);
    brightness = b < 16 ? 16 : b;

    showStatusBar = g_prefs.getBool(kKeyStatusBar, defaults.showStatusBar);
    globeNight = g_prefs.getBool(kKeyNight, defaults.globeNight);
    globeGraticule = g_prefs.getBool(kKeyGraticule, defaults.globeGraticule);
    globeBorders = g_prefs.getBool(kKeyBorders, defaults.globeBorders);
    globeLakes = g_prefs.getBool(kKeyLakes, defaults.globeLakes);

    g_prefs.end();
}

void Settings::save() {
    if (!g_prefs.begin(kPrefsNamespace, /*readOnly=*/false)) return;
    g_prefs.putUChar(kKeyFormat, static_cast<uint8_t>(coordFormat));
    g_prefs.putUChar(kKeyUnits, static_cast<uint8_t>(units));
    g_prefs.putChar(kKeyTz, tzQuarterHours);
    g_prefs.putUChar(kKeyBright, brightness);
    g_prefs.putBool(kKeyStatusBar, showStatusBar);
    g_prefs.putBool(kKeyNight, globeNight);
    g_prefs.putBool(kKeyGraticule, globeGraticule);
    g_prefs.putBool(kKeyBorders, globeBorders);
    g_prefs.putBool(kKeyLakes, globeLakes);
    g_prefs.end();
}

void Settings::restoreDefaults() {
    *this = Settings{};
    save();
}

double Settings::distanceFromMeters(double meters) const {
    switch (units) {
        case Units::Imperial: return meters / 1609.344;
        case Units::Nautical: return meters / 1852.0;
        default:              return meters / 1000.0;
    }
}

const char* Settings::distanceUnit() const {
    switch (units) {
        case Units::Imperial: return "mi";
        case Units::Nautical: return "NM";
        default:              return "km";
    }
}

double Settings::altitudeFromMeters(double meters) const {
    return units == Units::Metric ? meters : meters * 3.280839895;
}

const char* Settings::altitudeUnit() const {
    return units == Units::Metric ? "m" : "ft";
}

double Settings::speedFromKph(double kph) const {
    switch (units) {
        case Units::Imperial: return kph / 1.609344;
        case Units::Nautical: return kph / 1.852;
        default:              return kph;
    }
}

const char* Settings::speedUnit() const {
    switch (units) {
        case Units::Imperial: return "mph";
        case Units::Nautical: return "kn";
        default:              return "km/h";
    }
}

}  // namespace geoscout
