#include "geo.h"

#include <cmath>
#include <cstdio>

namespace geoscout {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

// WGS84, which is what the receiver reports and what UTM and MGRS below assume.
constexpr double kWgs84A = 6378137.0;
constexpr double kWgs84F = 1.0 / 298.257223563;
constexpr double kUtmK0 = 0.9996;

const char* const kCompass16[] = {
    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
    "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW",
};

// Band letters run C..X in 8-degree steps from 80S, with I and O omitted so
// they cannot be misread as 1 and 0. X is the odd one: it is 12 degrees tall.
const char kBandLetters[] = "CDEFGHJKLMNPQRSTUVWX";

}  // namespace

Coord Coord::fromDegrees(double lat, double lon) {
    Coord c;
    c.lat_e7 = static_cast<int32_t>(std::llround(lat * 1e7));
    c.lon_e7 = static_cast<int32_t>(std::llround(lon * 1e7));
    return c;
}

double Coord::latDeg() const { return static_cast<double>(lat_e7) / 1e7; }
double Coord::lonDeg() const { return static_cast<double>(lon_e7) / 1e7; }

bool coordValid(const Coord& c) {
    if (c.lat_e7 == 0 && c.lon_e7 == 0) return false;
    if (c.lat_e7 > 900000000 || c.lat_e7 < -900000000) return false;
    if (c.lon_e7 > 1800000000 || c.lon_e7 < -1800000000) return false;
    return true;
}

double distanceMeters(const Coord& a, const Coord& b) {
    const double lat1 = a.latDeg() * kDegToRad;
    const double lat2 = b.latDeg() * kDegToRad;
    const double dLat = lat2 - lat1;
    const double dLon = (b.lonDeg() - a.lonDeg()) * kDegToRad;

    const double sinLat = std::sin(dLat * 0.5);
    const double sinLon = std::sin(dLon * 0.5);
    double h = sinLat * sinLat + std::cos(lat1) * std::cos(lat2) * sinLon * sinLon;
    if (h > 1.0) h = 1.0;  // guard against rounding pushing asin out of domain
    return 2.0 * kEarthRadiusM * std::asin(std::sqrt(h));
}

double bearingDegrees(const Coord& a, const Coord& b) {
    const double lat1 = a.latDeg() * kDegToRad;
    const double lat2 = b.latDeg() * kDegToRad;
    const double dLon = (b.lonDeg() - a.lonDeg()) * kDegToRad;

    const double y = std::sin(dLon) * std::cos(lat2);
    const double x = std::cos(lat1) * std::sin(lat2) -
                     std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
    return normalizeDegrees(std::atan2(y, x) * kRadToDeg);
}

Coord antipode(const Coord& c) {
    return Coord::fromDegrees(-c.latDeg(), wrapLongitude(c.lonDeg() + 180.0));
}

double normalizeDegrees(double deg) {
    double d = std::fmod(deg, 360.0);
    if (d < 0.0) d += 360.0;
    return d;
}

double wrapLongitude(double deg) {
    double d = normalizeDegrees(deg);
    if (d >= 180.0) d -= 360.0;
    return d;
}

const char* compassPoint(double bearingDeg) {
    const double b = normalizeDegrees(bearingDeg);
    int idx = static_cast<int>(std::floor(b / 22.5 + 0.5));
    idx %= 16;
    return kCompass16[idx];
}

const char* coordFormatName(CoordFormat f) {
    switch (f) {
        case CoordFormat::DecimalDegrees: return "Decimal";
        case CoordFormat::DegMinSec:      return "D M S";
        case CoordFormat::DegDecimalMin:  return "D M.m";
        case CoordFormat::Maidenhead:     return "Maidenhead";
        case CoordFormat::Utm:            return "UTM";
        case CoordFormat::Mgrs:           return "MGRS";
        default:                          return "?";
    }
}

// --- axis formats ---------------------------------------------------------

namespace {

char hemisphere(double deg, bool isLatitude) {
    if (isLatitude) return deg < 0.0 ? 'S' : 'N';
    return deg < 0.0 ? 'W' : 'E';
}

bool wrote(int n, size_t outLen) {
    return n > 0 && static_cast<size_t>(n) < outLen;
}

}  // namespace

bool formatDecimal(double deg, bool isLatitude, char* out, size_t outLen) {
    if (out == nullptr || outLen == 0) return false;
    const char h = hemisphere(deg, isLatitude);
    const int n = std::snprintf(out, outLen, "%.6f %c", std::fabs(deg), h);
    return wrote(n, outLen);
}

bool formatDms(double deg, bool isLatitude, char* out, size_t outLen) {
    if (out == nullptr || outLen == 0) return false;
    const char h = hemisphere(deg, isLatitude);
    double a = std::fabs(deg);

    // Round once, in tenths of a second, and carry upward. Rounding each
    // component on its own is how you end up printing 60'00.0".
    long tenths = std::lround(a * 3600.0 * 10.0);
    const long secTenths = tenths % 600;
    tenths /= 600;
    const long minutes = tenths % 60;
    const long degrees = tenths / 60;

    const int n = std::snprintf(out, outLen, "%ld %02ld' %04.1f\" %c",
                                degrees, minutes,
                                static_cast<double>(secTenths) / 10.0, h);
    return wrote(n, outLen);
}

bool formatDdm(double deg, bool isLatitude, char* out, size_t outLen) {
    if (out == nullptr || outLen == 0) return false;
    const char h = hemisphere(deg, isLatitude);
    double a = std::fabs(deg);

    long thousandths = std::lround(a * 60.0 * 1000.0);
    const long minThousandths = thousandths % 60000;
    const long degrees = thousandths / 60000;

    const int n = std::snprintf(out, outLen, "%ld %06.3f' %c", degrees,
                                static_cast<double>(minThousandths) / 1000.0, h);
    return wrote(n, outLen);
}

// --- Maidenhead -----------------------------------------------------------

bool formatMaidenhead(const Coord& c, int precision, char* out, size_t outLen) {
    if (out == nullptr || outLen < 9) return false;
    if (precision < 4) precision = 4;
    if (precision > 8) precision = 8;
    precision &= ~1;  // only even lengths exist

    // Shift into the positive quadrant the locator system is defined over.
    double lon = wrapLongitude(c.lonDeg()) + 180.0;
    double lat = c.latDeg() + 90.0;
    if (lat < 0.0) lat = 0.0;
    if (lat > 180.0) lat = 180.0;
    if (lon >= 360.0) lon = 359.999999;

    int i = 0;
    out[i++] = static_cast<char>('A' + static_cast<int>(lon / 20.0));
    out[i++] = static_cast<char>('A' + static_cast<int>(lat / 10.0));
    lon = std::fmod(lon, 20.0);
    lat = std::fmod(lat, 10.0);

    out[i++] = static_cast<char>('0' + static_cast<int>(lon / 2.0));
    out[i++] = static_cast<char>('0' + static_cast<int>(lat / 1.0));
    lon = std::fmod(lon, 2.0);
    lat = std::fmod(lat, 1.0);

    if (precision >= 6) {
        out[i++] = static_cast<char>('a' + static_cast<int>(lon / (2.0 / 24.0)));
        out[i++] = static_cast<char>('a' + static_cast<int>(lat / (1.0 / 24.0)));
        lon = std::fmod(lon, 2.0 / 24.0);
        lat = std::fmod(lat, 1.0 / 24.0);
    }
    if (precision >= 8) {
        out[i++] = static_cast<char>('0' + static_cast<int>(lon / (2.0 / 240.0)));
        out[i++] = static_cast<char>('0' + static_cast<int>(lat / (1.0 / 240.0)));
    }
    out[i] = '\0';
    return true;
}

// --- UTM and MGRS ---------------------------------------------------------

char utmBand(double latDeg) {
    if (latDeg < -80.0 || latDeg >= 84.0) return '\0';
    int idx = static_cast<int>(std::floor((latDeg + 80.0) / 8.0));
    if (idx > 19) idx = 19;   // band X is twelve degrees tall, not eight
    if (idx < 0) idx = 0;
    return kBandLetters[idx];
}

UtmCoord toUtm(const Coord& c) {
    UtmCoord u;
    const double latDeg = c.latDeg();
    const double lonDeg = wrapLongitude(c.lonDeg());

    u.band = utmBand(latDeg);
    if (u.band == '\0') return u;   // polar: UPS territory, not handled

    int zone = static_cast<int>(std::floor((lonDeg + 180.0) / 6.0)) + 1;
    if (zone < 1) zone = 1;
    if (zone > 60) zone = 60;

    // The two documented irregularities: southwest Norway widens zone 32, and
    // Svalbard redraws four zone boundaries entirely.
    if (latDeg >= 56.0 && latDeg < 64.0 && lonDeg >= 3.0 && lonDeg < 12.0) {
        zone = 32;
    } else if (latDeg >= 72.0 && latDeg < 84.0) {
        if (lonDeg >= 0.0 && lonDeg < 9.0) zone = 31;
        else if (lonDeg >= 9.0 && lonDeg < 21.0) zone = 33;
        else if (lonDeg >= 21.0 && lonDeg < 33.0) zone = 35;
        else if (lonDeg >= 33.0 && lonDeg < 42.0) zone = 37;
    }

    const double e2 = kWgs84F * (2.0 - kWgs84F);
    const double ep2 = e2 / (1.0 - e2);

    const double phi = latDeg * kDegToRad;
    const double lambda = lonDeg * kDegToRad;
    const double lambda0 = ((zone - 1) * 6 - 180 + 3) * kDegToRad;

    const double sinPhi = std::sin(phi);
    const double cosPhi = std::cos(phi);
    const double tanPhi = std::tan(phi);

    const double N = kWgs84A / std::sqrt(1.0 - e2 * sinPhi * sinPhi);
    const double T = tanPhi * tanPhi;
    const double C = ep2 * cosPhi * cosPhi;
    const double A = (lambda - lambda0) * cosPhi;

    const double e4 = e2 * e2;
    const double e6 = e4 * e2;
    const double M = kWgs84A *
        ((1.0 - e2 / 4.0 - 3.0 * e4 / 64.0 - 5.0 * e6 / 256.0) * phi -
         (3.0 * e2 / 8.0 + 3.0 * e4 / 32.0 + 45.0 * e6 / 1024.0) * std::sin(2.0 * phi) +
         (15.0 * e4 / 256.0 + 45.0 * e6 / 1024.0) * std::sin(4.0 * phi) -
         (35.0 * e6 / 3072.0) * std::sin(6.0 * phi));

    const double A2 = A * A;
    const double A3 = A2 * A;
    const double A4 = A3 * A;
    const double A5 = A4 * A;
    const double A6 = A5 * A;

    u.easting = kUtmK0 * N *
        (A + (1.0 - T + C) * A3 / 6.0 +
         (5.0 - 18.0 * T + T * T + 72.0 * C - 58.0 * ep2) * A5 / 120.0) + 500000.0;

    u.northing = kUtmK0 *
        (M + N * tanPhi *
             (A2 / 2.0 + (5.0 - T + 9.0 * C + 4.0 * C * C) * A4 / 24.0 +
              (61.0 - 58.0 * T + T * T + 600.0 * C - 330.0 * ep2) * A6 / 720.0));

    u.northern = latDeg >= 0.0;
    if (!u.northern) u.northing += 10000000.0;

    u.zone = zone;
    u.valid = true;
    return u;
}

bool formatUtm(const Coord& c, char* out, size_t outLen) {
    if (out == nullptr || outLen == 0) return false;
    const UtmCoord u = toUtm(c);
    if (!u.valid) {
        std::snprintf(out, outLen, "polar (no UTM)");
        return false;
    }
    const int n = std::snprintf(out, outLen, "%d%c %.0fE %.0fN",
                                u.zone, u.band, u.easting, u.northing);
    return wrote(n, outLen);
}

bool formatMgrs(const Coord& c, char* out, size_t outLen) {
    if (out == nullptr || outLen == 0) return false;
    const UtmCoord u = toUtm(c);
    if (!u.valid) {
        std::snprintf(out, outLen, "polar (no MGRS)");
        return false;
    }

    // The 100 km square identifier. Column letters cycle through three sets as
    // the zone number advances; row letters cycle through twenty and are shifted
    // half a cycle on even zones, which is what keeps neighbouring squares from
    // sharing a name.
    static const char* const kCols[3] = {"ABCDEFGH", "JKLMNPQR", "STUVWXYZ"};
    static const char kRows[] = "ABCDEFGHJKLMNPQRSTUV";

    const long east100k = static_cast<long>(u.easting / 100000.0);
    const long north100k = static_cast<long>(u.northing / 100000.0);

    int colIdx = static_cast<int>(east100k) - 1;
    if (colIdx < 0) colIdx = 0;
    if (colIdx > 7) colIdx = 7;
    const char colLetter = kCols[(u.zone - 1) % 3][colIdx];

    int rowIdx = static_cast<int>(north100k % 20);
    if (u.zone % 2 == 0) rowIdx = (rowIdx + 5) % 20;
    const char rowLetter = kRows[rowIdx];

    // Truncated, not rounded. A grid reference names the square you are
    // standing in; rounding would move you into the next one at its edge, and
    // would disagree with every other MGRS implementation by a metre.
    const long e = static_cast<long>(u.easting) % 100000;
    const long n100 = static_cast<long>(u.northing) % 100000;

    const int n = std::snprintf(out, outLen, "%d%c %c%c %05ld %05ld",
                                u.zone, u.band, colLetter, rowLetter, e, n100);
    return wrote(n, outLen);
}

}  // namespace geoscout
