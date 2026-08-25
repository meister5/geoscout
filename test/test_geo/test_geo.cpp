#include "../support/check.h"
#include "geo.h"

using namespace geoscout;

namespace {

// Reference positions whose UTM and MGRS values were taken from pyproj (which
// wraps PROJ) and the NGA-derived `mgrs` library, so these conversions are
// checked against implementations outside this repository rather than against
// themselves.
const Coord kWashingtonMonument = Coord::fromDegrees(38.889484, -77.035278);
// Newington, Connecticut -- the locator every amateur radio primer uses.
const Coord kNewington = Coord::fromDegrees(41.7143, -72.7284);

const Coord kAmsterdam = Coord::fromDegrees(52.373056, 4.892222);
const Coord kSydney = Coord::fromDegrees(-33.856778, 151.215278);

}  // namespace

int main() {
    char buf[40];

    // --- coordinate storage -------------------------------------------------
    {
        const Coord c = Coord::fromDegrees(52.373056, 4.892222);
        CHECK_NEAR(c.latDeg(), 52.373056, 1e-7);
        CHECK_NEAR(c.lonDeg(), 4.892222, 1e-7);

        CHECK_TRUE(coordValid(c));
        CHECK_FALSE(coordValid(Coord{}));                       // unset struct
        CHECK_FALSE(coordValid(Coord::fromDegrees(91.0, 0.0)));
        CHECK_FALSE(coordValid(Coord::fromDegrees(0.0, 181.0)));
    }

    // --- distance and bearing ----------------------------------------------
    {
        // Amsterdam to Sydney is a well-known 16,650 km great circle.
        const double d = distanceMeters(kAmsterdam, kSydney);
        CHECK_NEAR(d / 1000.0, 16650.0, 40.0);

        // A degree of latitude is 111.2 km anywhere.
        CHECK_NEAR(distanceMeters(Coord::fromDegrees(0.0, 10.0),
                                  Coord::fromDegrees(1.0, 10.0)) / 1000.0,
                   111.2, 0.5);

        CHECK_NEAR(distanceMeters(kAmsterdam, kAmsterdam), 0.0, 0.001);

        // Due north, due east.
        CHECK_NEAR(bearingDegrees(Coord::fromDegrees(0.0, 0.0),
                                  Coord::fromDegrees(1.0, 0.0)), 0.0, 0.01);
        CHECK_NEAR(bearingDegrees(Coord::fromDegrees(0.0, 0.0),
                                  Coord::fromDegrees(0.0, 1.0)), 90.0, 0.01);
        CHECK_NEAR(bearingDegrees(Coord::fromDegrees(0.0, 0.0),
                                  Coord::fromDegrees(-1.0, 0.0)), 180.0, 0.01);
        CHECK_NEAR(bearingDegrees(Coord::fromDegrees(0.0, 0.0),
                                  Coord::fromDegrees(0.0, -1.0)), 270.0, 0.01);
    }

    // --- compass points -----------------------------------------------------
    {
        CHECK_STREQ(compassPoint(0.0), "N");
        CHECK_STREQ(compassPoint(359.0), "N");     // must wrap, not clamp
        CHECK_STREQ(compassPoint(45.0), "NE");
        CHECK_STREQ(compassPoint(90.0), "E");
        CHECK_STREQ(compassPoint(202.5), "SSW");
        CHECK_STREQ(compassPoint(-90.0), "W");
    }

    // --- antipode -----------------------------------------------------------
    {
        const Coord a = antipode(kAmsterdam);
        CHECK_NEAR(a.latDeg(), -52.373056, 1e-5);
        CHECK_NEAR(a.lonDeg(), -175.107778, 1e-5);
        // Half the circumference away, by definition.
        CHECK_NEAR(distanceMeters(kAmsterdam, a) / 1000.0, 20015.0, 5.0);
    }

    // --- angle wrapping -----------------------------------------------------
    {
        CHECK_NEAR(normalizeDegrees(-1.0), 359.0, 1e-9);
        CHECK_NEAR(normalizeDegrees(721.0), 1.0, 1e-9);
        CHECK_NEAR(wrapLongitude(190.0), -170.0, 1e-9);
        CHECK_NEAR(wrapLongitude(-190.0), 170.0, 1e-9);
        CHECK_NEAR(wrapLongitude(180.0), -180.0, 1e-9);
    }

    // --- decimal, DMS and DDM ----------------------------------------------
    {
        CHECK_TRUE(formatDecimal(52.373056, true, buf, sizeof(buf)));
        CHECK_STREQ(buf, "52.373056 N");
        CHECK_TRUE(formatDecimal(-33.856778, true, buf, sizeof(buf)));
        CHECK_STREQ(buf, "33.856778 S");
        CHECK_TRUE(formatDecimal(-77.035278, false, buf, sizeof(buf)));
        CHECK_STREQ(buf, "77.035278 W");

        CHECK_TRUE(formatDms(52.373056, true, buf, sizeof(buf)));
        CHECK_STREQ(buf, "52 22' 23.0\" N");
        CHECK_TRUE(formatDms(4.892222, false, buf, sizeof(buf)));
        CHECK_STREQ(buf, "4 53' 32.0\" E");

        // Rounding must carry rather than print sixty seconds.
        CHECK_TRUE(formatDms(1.9999999, true, buf, sizeof(buf)));
        CHECK_STREQ(buf, "2 00' 00.0\" N");

        CHECK_TRUE(formatDdm(52.373056, true, buf, sizeof(buf)));
        CHECK_STREQ(buf, "52 22.383' N");
        // Same carry problem, one unit up.
        CHECK_TRUE(formatDdm(-0.9999999, false, buf, sizeof(buf)));
        CHECK_STREQ(buf, "1 00.000' W");

        // A buffer too small must fail rather than emit a truncated position.
        char tiny[4];
        CHECK_FALSE(formatDecimal(52.373056, true, tiny, sizeof(tiny)));
    }

    // --- Maidenhead ---------------------------------------------------------
    {
        CHECK_TRUE(formatMaidenhead(kNewington, 6, buf, sizeof(buf)));
        CHECK_STREQ(buf, "FN31pr");
        CHECK_TRUE(formatMaidenhead(kNewington, 4, buf, sizeof(buf)));
        CHECK_STREQ(buf, "FN31");
        CHECK_TRUE(formatMaidenhead(kNewington, 8, buf, sizeof(buf)));
        CHECK_CONTAINS(buf, "FN31pr");

        // Amsterdam sits in JO22.
        CHECK_TRUE(formatMaidenhead(kAmsterdam, 6, buf, sizeof(buf)));
        CHECK_CONTAINS(buf, "JO22");

        // Sydney, southern hemisphere and east of the antimeridian half.
        CHECK_TRUE(formatMaidenhead(kSydney, 6, buf, sizeof(buf)));
        CHECK_CONTAINS(buf, "QF56");

        // The origin of the grid, and the far corner, must not run off the end.
        CHECK_TRUE(formatMaidenhead(Coord::fromDegrees(-90.0, -180.0), 6, buf, sizeof(buf)));
        CHECK_STREQ(buf, "AA00aa");
        CHECK_TRUE(formatMaidenhead(Coord::fromDegrees(89.999, 179.999), 6, buf, sizeof(buf)));
        CHECK_STREQ(buf, "RR99xx");
    }

    // --- UTM ----------------------------------------------------------------
    {
        const UtmCoord u = toUtm(kWashingtonMonument);
        CHECK_TRUE(u.valid);
        CHECK_EQ(u.zone, 18);
        CHECK_EQ(static_cast<int>(u.band), static_cast<int>('S'));
        // PROJ: 323479.932, 4306481.423. Agreement to a centimetre says the
        // series expansion is carried far enough.
        CHECK_NEAR(u.easting, 323479.932, 0.01);
        CHECK_NEAR(u.northing, 4306481.423, 0.01);
        CHECK_TRUE(u.northern);

        // Southern hemisphere gets the ten-million-metre false northing.
        const UtmCoord s = toUtm(kSydney);
        CHECK_TRUE(s.valid);
        CHECK_EQ(s.zone, 56);
        CHECK_FALSE(s.northern);
        CHECK_NEAR(s.easting, 334898.49, 0.02);
        CHECK_NEAR(s.northing, 6252291.16, 0.02);

        // And one in between, to catch a sign or hemisphere slip that the
        // extremes would hide.
        const UtmCoord a = toUtm(kAmsterdam);
        CHECK_EQ(a.zone, 31);
        CHECK_NEAR(a.easting, 628813.54, 0.02);
        CHECK_NEAR(a.northing, 5804216.79, 0.02);

        // Southwest Norway widens zone 32; Svalbard redraws four boundaries.
        CHECK_EQ(toUtm(Coord::fromDegrees(60.0, 5.0)).zone, 32);
        CHECK_EQ(toUtm(Coord::fromDegrees(78.0, 15.0)).zone, 33);
        CHECK_EQ(toUtm(Coord::fromDegrees(78.0, 25.0)).zone, 35);

        // The poles are outside UTM entirely and must say so.
        CHECK_FALSE(toUtm(Coord::fromDegrees(85.0, 0.0)).valid);
        CHECK_FALSE(toUtm(Coord::fromDegrees(-81.0, 0.0)).valid);
        CHECK_EQ(static_cast<int>(utmBand(-80.0)), static_cast<int>('C'));
        CHECK_EQ(static_cast<int>(utmBand(0.0)), static_cast<int>('N'));
        CHECK_EQ(static_cast<int>(utmBand(83.0)), static_cast<int>('X'));
        CHECK_EQ(static_cast<int>(utmBand(84.0)), 0);

        CHECK_TRUE(formatUtm(kWashingtonMonument, buf, sizeof(buf)));
        CHECK_CONTAINS(buf, "18S");
        CHECK_CONTAINS(buf, "323480E");
    }

    // --- MGRS ---------------------------------------------------------------
    //
    // Every expectation here came out of the `mgrs` library. The 100 km square
    // letters are the part worth checking hardest: the column set rotates with
    // the zone number and the row set is shifted half a cycle on even zones, so
    // getting either wrong still produces a plausible-looking reference.
    {
        struct Case { Coord c; const char* want; };
        const Case cases[] = {
            {kWashingtonMonument,                    "18S UJ 23479 06481"},
            {kAmsterdam,                             "31U FU 28813 04216"},
            {kSydney,                                "56H LH 34898 52291"},
            {kNewington,                             "18T XM 88973 20549"},
            {Coord::fromDegrees(60.0, 5.0),          "32V KM 76979 58157"},
            {Coord::fromDegrees(78.0, 15.0),         "33X WG 00000 58369"},
            {Coord::fromDegrees(-45.0, -70.0),       "19G DL 21184 16563"},
            {Coord::fromDegrees(1.0, 1.0),           "31N BB 77438 10597"},
            {Coord::fromDegrees(-1.0, -1.0),         "30M YD 22561 89402"},
            {Coord::fromDegrees(51.5004, -0.1246),   "30U XC 99568 09394"},
        };
        for (const Case& test : cases) {
            CHECK_TRUE(formatMgrs(test.c, buf, sizeof(buf)));
            CHECK_STREQ(buf, test.want);
        }

        // Refused, not guessed, at the poles.
        CHECK_FALSE(formatMgrs(Coord::fromDegrees(89.0, 0.0), buf, sizeof(buf)));
        CHECK_CONTAINS(buf, "polar");
    }

    // --- format names -------------------------------------------------------
    for (uint8_t i = 0; i < static_cast<uint8_t>(CoordFormat::Count); ++i) {
        CHECK_TRUE(coordFormatName(static_cast<CoordFormat>(i))[0] != '?');
    }

    return check::finish("geo");
}
