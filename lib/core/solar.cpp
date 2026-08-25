#include "solar.h"

#include <cmath>

#include "geo.h"

namespace geoscout {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

// Julian date of the Unix epoch.
constexpr double kJdUnixEpoch = 2440587.5;
constexpr double kJ2000 = 2451545.0;

}  // namespace

SubsolarPoint subsolarPoint(uint64_t unixSeconds) {
    SubsolarPoint p;
    if (unixSeconds == 0) return p;

    // Days since J2000, the epoch every term below is expressed against.
    const double n = (static_cast<double>(unixSeconds) / 86400.0 + kJdUnixEpoch) - kJ2000;

    // Low-precision solar position, per the Astronomical Almanac.
    const double meanLon = normalizeDegrees(280.460 + 0.9856474 * n);
    const double meanAnom = normalizeDegrees(357.528 + 0.9856003 * n) * kDegToRad;

    // Equation of centre folded in: mean longitude becomes apparent ecliptic
    // longitude, which is what actually sets the declination.
    const double eclipticLon =
        (meanLon + 1.915 * std::sin(meanAnom) + 0.020 * std::sin(2.0 * meanAnom)) * kDegToRad;
    const double obliquity = (23.439 - 0.0000004 * n) * kDegToRad;

    const double sinDec = std::sin(obliquity) * std::sin(eclipticLon);
    const double declination = std::asin(sinDec) * kRadToDeg;

    const double rightAscension =
        std::atan2(std::cos(obliquity) * std::sin(eclipticLon), std::cos(eclipticLon)) * kRadToDeg;

    // Greenwich mean sidereal time tells us which meridian is currently facing
    // the point on the celestial sphere the sun occupies.
    const double gmstHours = std::fmod(18.697374558 + 24.06570982441908 * n, 24.0);
    const double gmstDeg = normalizeDegrees(gmstHours * 15.0);

    p.latDeg = declination;
    p.lonDeg = wrapLongitude(rightAscension - gmstDeg);
    p.valid = true;
    return p;
}

double sunAltitudeDeg(const SubsolarPoint& sun, double latDeg, double lonDeg) {
    if (!sun.valid) return 0.0;
    const double lat = latDeg * kDegToRad;
    const double dec = sun.latDeg * kDegToRad;
    const double hourAngle = (lonDeg - sun.lonDeg) * kDegToRad;

    double sinAlt = std::sin(lat) * std::sin(dec) +
                    std::cos(lat) * std::cos(dec) * std::cos(hourAngle);
    if (sinAlt > 1.0) sinAlt = 1.0;
    if (sinAlt < -1.0) sinAlt = -1.0;
    return std::asin(sinAlt) * kRadToDeg;
}

const char* daylightPhase(double altitude) {
    if (altitude >= 10.0) return "Day";
    if (altitude >= 6.0) return "Golden hour";
    if (altitude >= -0.833) return "Sunrise/set";
    if (altitude >= -6.0) return "Civil twilight";
    if (altitude >= -12.0) return "Nautical twilight";
    if (altitude >= -18.0) return "Astro twilight";
    return "Night";
}

}  // namespace geoscout
