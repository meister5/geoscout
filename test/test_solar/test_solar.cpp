#include "../support/check.h"
#include "solar.h"

#include <cmath>

using namespace geoscout;

int main() {
    // --- an unset clock says so --------------------------------------------
    {
        const SubsolarPoint p = subsolarPoint(0);
        CHECK_FALSE(p.valid);
        CHECK_NEAR(sunAltitudeDeg(p, 52.0, 5.0), 0.0, 1e-9);
    }

    // --- solstices and equinoxes -------------------------------------------
    //
    // Declination is the sharpest check available: it is zero at the equinoxes
    // and reaches the obliquity of the ecliptic at the solstices, and nothing
    // about the rest of the calculation can be wrong while those hold.
    {
        // 2026-03-20 14:46 UTC, the March equinox.
        const SubsolarPoint equinox = subsolarPoint(1774017960ull);
        CHECK_TRUE(equinox.valid);
        CHECK_NEAR(equinox.latDeg, 0.0, 0.05);

        // 2026-06-21 08:24 UTC, the June solstice.
        const SubsolarPoint june = subsolarPoint(1781943840ull);
        CHECK_NEAR(june.latDeg, 23.44, 0.05);

        // 2026-12-21 20:50 UTC, the December solstice.
        const SubsolarPoint december = subsolarPoint(1797713400ull);
        CHECK_NEAR(december.latDeg, -23.44, 0.05);
    }

    // --- the subsolar longitude tracks the clock ---------------------------
    {
        // At 12:00 UTC the sun is near the Greenwich meridian, offset only by
        // the equation of time -- at most about a quarter of an hour, so four
        // degrees.
        const uint64_t noonUtc = 1774094400ull;   // 2026-03-21 12:00:00 UTC
        const SubsolarPoint noon = subsolarPoint(noonUtc);
        CHECK_TRUE(std::fabs(noon.lonDeg) < 4.5);

        // Six hours later it has moved 90 degrees west.
        const SubsolarPoint later = subsolarPoint(noonUtc + 6 * 3600);
        double moved = noon.lonDeg - later.lonDeg;
        if (moved < 0.0) moved += 360.0;
        CHECK_NEAR(moved, 90.0, 0.5);

        // A full day returns it to within a degree of where it started.
        const SubsolarPoint tomorrow = subsolarPoint(noonUtc + 86400);
        CHECK_NEAR(tomorrow.lonDeg, noon.lonDeg, 1.0);

        // And it always stays a legal longitude.
        for (uint64_t t = noonUtc; t < noonUtc + 86400; t += 997) {
            const SubsolarPoint p = subsolarPoint(t);
            CHECK_TRUE(p.lonDeg >= -180.0 && p.lonDeg < 180.0);
        }
    }

    // --- sun altitude -------------------------------------------------------
    {
        const SubsolarPoint sun = subsolarPoint(1774094400ull);

        // Directly overhead at the subsolar point itself.
        CHECK_NEAR(sunAltitudeDeg(sun, sun.latDeg, sun.lonDeg), 90.0, 0.01);
        // Directly underfoot at its antipode.
        double antiLon = sun.lonDeg + 180.0;
        if (antiLon >= 180.0) antiLon -= 360.0;
        CHECK_NEAR(sunAltitudeDeg(sun, -sun.latDeg, antiLon), -90.0, 0.01);
        // On the terminator, ninety degrees away along the equator.
        CHECK_NEAR(sunAltitudeDeg(sun, sun.latDeg, sun.lonDeg + 90.0), 0.0, 0.01);

        // Amsterdam at local midnight in midwinter is thoroughly dark.
        const SubsolarPoint midwinter = subsolarPoint(1797724800ull);  // 2026-12-22 00:00 UTC
        CHECK_TRUE(sunAltitudeDeg(midwinter, 52.37, 4.89) < -18.0);
    }

    // --- daylight phases ----------------------------------------------------
    {
        CHECK_STREQ(daylightPhase(45.0), "Day");
        CHECK_STREQ(daylightPhase(7.0), "Golden hour");
        CHECK_STREQ(daylightPhase(0.0), "Sunrise/set");
        CHECK_STREQ(daylightPhase(-3.0), "Civil twilight");
        CHECK_STREQ(daylightPhase(-9.0), "Nautical twilight");
        CHECK_STREQ(daylightPhase(-15.0), "Astro twilight");
        CHECK_STREQ(daylightPhase(-40.0), "Night");
    }

    return check::finish("solar");
}
