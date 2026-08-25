// Geodesy and coordinate formatting. No hardware, no Arduino: this header must
// stay compilable on the host so the native tests can exercise it.
#pragma once

#include <cstddef>
#include <cstdint>

namespace geoscout {

// Mean Earth radius (IUGG). Haversine on a sphere is good to ~0.5% which is far
// below the noise in a handheld GNSS fix, so an ellipsoidal model would be
// precision we cannot actually measure.
constexpr double kEarthRadiusM = 6371008.8;

// Coordinates are stored as degrees * 1e7 in int32. That is ~1.1 cm of
// resolution, well under the receiver's 1.5 m CEP50, and it keeps every stored
// waypoint a fixed 8 bytes with no float drift.
struct Coord {
    int32_t lat_e7 = 0;
    int32_t lon_e7 = 0;

    static Coord fromDegrees(double lat, double lon);
    double latDeg() const;
    double lonDeg() const;
};

// A fix at exactly (0,0) is overwhelmingly likely to be an unset struct rather
// than a boat off Ghana, so it is treated as invalid along with out-of-range.
bool coordValid(const Coord& c);

double distanceMeters(const Coord& a, const Coord& b);

// Initial great-circle bearing from a to b, degrees clockwise from true north.
double bearingDegrees(const Coord& a, const Coord& b);

// The point diametrically opposite on the sphere.
Coord antipode(const Coord& c);

// Wraps any angle into [0, 360).
double normalizeDegrees(double deg);

// Wraps a longitude into [-180, 180).
double wrapLongitude(double deg);

// "NNE", "SW" and so on. 16-point compass, never null.
const char* compassPoint(double bearingDeg);

// ---------------------------------------------------------------------------
// Formatting
//
// Every one of these writes a null-terminated string and returns false without
// writing anything meaningful if the buffer is too small. Latitude and
// longitude are formatted separately because the display lays them out on
// their own lines.
// ---------------------------------------------------------------------------

enum class CoordFormat : uint8_t {
    DecimalDegrees = 0,   // 52.379189 N
    DegMinSec,            // 52 22' 45.1" N
    DegDecimalMin,        // 52 22.752' N
    Maidenhead,           // JO22NC
    Utm,                  // 31U 628344E 5804073N
    Mgrs,                 // 31U FU 28344 04073
    Count,
};

const char* coordFormatName(CoordFormat f);

// The two axis-wise formats. `isLatitude` picks N/S versus E/W.
bool formatDecimal(double deg, bool isLatitude, char* out, size_t outLen);
bool formatDms(double deg, bool isLatitude, char* out, size_t outLen);
bool formatDdm(double deg, bool isLatitude, char* out, size_t outLen);

// The grid formats describe a position, not an axis, so they produce one
// string for the pair.
//
// Maidenhead locator. `precision` is the character count: 4, 6 or 8. Anything
// else is clamped into that range. Needs 9 bytes.
bool formatMaidenhead(const Coord& c, int precision, char* out, size_t outLen);

// UTM. Returns false outside +/-80 degrees latitude, where UTM is not defined
// and UPS takes over -- the polar regions are left unhandled deliberately
// rather than silently wrong. Needs 24 bytes.
bool formatUtm(const Coord& c, char* out, size_t outLen);

// MGRS to 1 m precision, same latitude limits as UTM. Needs 20 bytes.
bool formatMgrs(const Coord& c, char* out, size_t outLen);

// ---------------------------------------------------------------------------
// UTM, exposed because MGRS is built on it and both are worth testing directly.
// ---------------------------------------------------------------------------

struct UtmCoord {
    int zone = 0;
    char band = '\0';      // latitude band letter, C..X omitting I and O
    double easting = 0.0;
    double northing = 0.0;
    bool northern = true;
    bool valid = false;
};

UtmCoord toUtm(const Coord& c);

// The latitude band letter for a latitude in degrees, or '\0' outside
// [-80, 84).
char utmBand(double latDeg);

}  // namespace geoscout
