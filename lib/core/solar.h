// Where the sun is, from the clock the satellites give us.
//
// This is what draws the terminator. The subsolar point is the one place on
// Earth where the sun is directly overhead at a given instant; every point
// whose unit vector has a positive dot product with it is in daylight, which
// makes day/night a sign test per vertex rather than a shading pass.
//
// Accuracy is around a hundredth of a degree over 1950-2050, which is far
// better than a 240-pixel globe can show.
//
// Host-testable: no Arduino, no display library.
#pragma once

#include <cstdint>

namespace geoscout {

struct SubsolarPoint {
    double latDeg = 0.0;    // solar declination
    double lonDeg = 0.0;    // where local apparent noon is right now
    bool valid = false;
};

// `unixSeconds` is UTC, as the receiver reports it. Returns an invalid point
// for zero, which is what an unset clock looks like.
SubsolarPoint subsolarPoint(uint64_t unixSeconds);

// Sun altitude above the horizon at a place, in degrees. Negative is night;
// below -18 is astronomical darkness.
double sunAltitudeDeg(const SubsolarPoint& sun, double latDeg, double lonDeg);

// Civil daylight description for a sun altitude: "Day", "Golden hour",
// "Civil twilight", "Night" and so on. Never null.
const char* daylightPhase(double sunAltitudeDeg);

}  // namespace geoscout
