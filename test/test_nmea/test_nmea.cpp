#include "../support/check.h"
#include "nmea.h"

#include <cstring>

using namespace geoscout;

namespace {
void feed(NmeaParser& p, const char* sentence) {
    for (const char* c = sentence; *c; ++c) p.encode(*c);
    p.encode('\r');
    p.encode('\n');
}

// A real multi-constellation burst from an ATGM336H, checksums included.
const char* kRmc =
    "$GNRMC,134502.30,A,5130.0240,N,00007.4760,W,0.52,148.1,240826,,,A*62";
const char* kGga =
    "$GNGGA,134502.30,5130.0240,N,00007.4760,W,1,09,0.94,45.2,M,45.9,M,,*6E";
const char* kGsa = "$GNGSA,A,3,04,07,08,09,16,21,26,27,,,,,1.72,0.94,1.44*13";
const char* kGpGsv1 =
    "$GPGSV,2,1,07,04,55,120,42,07,33,210,38,08,17,290,31,09,71,050,45*7F";
const char* kGpGsv2 = "$GPGSV,2,2,07,16,25,180,29,21,44,310,40,26,12,065,22*4D";
const char* kGlGsv =
    "$GLGSV,1,1,04,68,40,150,35,69,22,200,28,70,60,310,41,71,15,090,19*6B";
const char* kRmcNoFix = "$GNRMC,134503.00,V,,,,,,,240826,,,N*69";
}  // namespace

int main() {
    // Checksum validation, with and without the leading '$'.
    CHECK_TRUE(nmeaChecksumValid(kRmc));
    CHECK_TRUE(nmeaChecksumValid(kGga + 1));
    CHECK_FALSE(nmeaChecksumValid(
        "$GNRMC,134502.30,A,5130.0240,N,00007.4760,W,0.52,148.1,240826,,,A*63"));
    CHECK_FALSE(nmeaChecksumValid("$GNRMC,no,star,here"));
    CHECK_FALSE(nmeaChecksumValid("$GNRMC,bad,hex*ZZ"));
    CHECK_FALSE(nmeaChecksumValid(nullptr));

    // ddmm.mmmm conversion, both hemispheres.
    double deg = 0.0;
    CHECK_TRUE(parseNmeaCoordinate("5130.0240", 'N', &deg));
    CHECK_NEAR(deg, 51.50040, 1e-6);
    CHECK_TRUE(parseNmeaCoordinate("00007.4760", 'W', &deg));
    CHECK_NEAR(deg, -0.12460, 1e-6);
    CHECK_TRUE(parseNmeaCoordinate("00007.4760", 'E', &deg));
    CHECK_NEAR(deg, 0.12460, 1e-6);
    CHECK_TRUE(parseNmeaCoordinate("3745.1234", 'S', &deg));
    CHECK_NEAR(deg, -37.752057, 1e-5);
    // Malformed input must be refused, not silently turned into a position.
    CHECK_FALSE(parseNmeaCoordinate("", 'N', &deg));
    CHECK_FALSE(parseNmeaCoordinate("12", 'N', &deg));
    CHECK_FALSE(parseNmeaCoordinate("5199.0000", 'N', &deg));  // 99 minutes

    NmeaParser p;
    feed(p, kRmc);
    feed(p, kGga);
    feed(p, kGsa);
    feed(p, kGpGsv1);
    feed(p, kGpGsv2);
    feed(p, kGlGsv);

    const GnssFix& fix = p.fix();
    CHECK_TRUE(fix.valid);
    CHECK_TRUE(fix.fixType == FixType::Fix3D);
    CHECK_NEAR(fix.coord.latDeg(), 51.50040, 1e-6);
    CHECK_NEAR(fix.coord.lonDeg(), -0.12460, 1e-6);
    CHECK_NEAR(fix.altitudeM, 45.2, 0.01);
    CHECK_NEAR(fix.geoidSepM, 45.9, 0.01);
    CHECK_EQ(fix.satsUsed, 9);
    CHECK_NEAR(fix.hdop, 0.94, 0.001);
    CHECK_NEAR(fix.pdop, 1.72, 0.001);
    CHECK_NEAR(fix.vdop, 1.44, 0.001);
    CHECK_NEAR(fix.speedKph, 0.52 * 1.852, 0.001);
    CHECK_NEAR(fix.courseDeg, 148.1, 0.001);

    // Time and date from RMC.
    CHECK_TRUE(fix.time.valid);
    CHECK_EQ(fix.time.year, 2026);
    CHECK_EQ(fix.time.month, 8);
    CHECK_EQ(fix.time.day, 24);
    CHECK_EQ(fix.time.hour, 13);
    CHECK_EQ(fix.time.minute, 45);
    CHECK_EQ(fix.time.second, 2);
    CHECK_EQ(fix.time.centisecond, 30);
    char iso[32] = {};
    CHECK_TRUE(fix.time.toIso8601(iso, sizeof(iso)));
    CHECK_STREQ(iso, "2026-08-24T13:45:02.30Z");

    // GPS and GLONASS in-view counts must sum, not overwrite: 7 + 4.
    CHECK_EQ(fix.satsInView, 11);
    // ...and carrier-to-noise must combine across both constellations.
    // GPS 42+38+31+45+29+40+22 = 247 over 7, GLONASS 35+28+41+19 = 123 over 4.
    CHECK_NEAR(fix.meanCn0, 370.0 / 11.0, 0.001);
    CHECK_EQ(fix.maxCn0, 45);

    CHECK_TRUE(fix.usable());
    CHECK_TRUE(p.checksumErrors() == 0);
    CHECK_TRUE(p.sentencesValid() == 6);

    // A void RMC clears the fix so stale coordinates cannot be logged as live.
    feed(p, kRmcNoFix);
    CHECK_FALSE(p.fix().valid);
    CHECK_FALSE(p.fix().usable());

    // A corrupt sentence is counted, not acted on.
    NmeaParser q;
    feed(q, kRmc);
    const int32_t before = q.fix().coord.lat_e7;
    feed(q, "$GNRMC,134502.30,A,9999.9999,N,00007.4760,W,0.52,148.1,240826,,,A*11");
    CHECK_EQ(q.checksumErrors(), 1u);
    CHECK_EQ(q.fix().coord.lat_e7, before);

    // A dropped byte mid-sentence must not poison the next one.
    NmeaParser r;
    for (const char* c = "$GNRMC,1345"; *c; ++c) r.encode(*c);  // truncated
    feed(r, kRmc);
    CHECK_TRUE(r.fix().valid);
    CHECK_NEAR(r.fix().coord.latDeg(), 51.50040, 1e-6);

    // Oversized garbage is dropped without overflowing the buffer.
    NmeaParser s;
    s.encode('$');
    for (int i = 0; i < 400; ++i) s.encode('A');
    CHECK_TRUE(s.overruns() >= 1);
    feed(s, kRmc);
    CHECK_TRUE(s.fix().valid);

    // A 2D fix at poor HDOP is not worth acting on.
    NmeaParser w;
    feed(w, kRmc);
    feed(w, "$GNGSA,A,2,04,07,,,,,,,,,,,25.0,18.5,15.0*11");
    CHECK_TRUE(w.fix().valid);
    CHECK_FALSE(w.fix().usable());

    // GNSS epoch time, which is what the terminator is drawn from. A fresh
    // parser, because the void-RMC fixture above deliberately advances the
    // clock to 13:45:03 -- time stays valid even when the position does not.
    {
        NmeaParser e;
        feed(e, kRmc);
        // 2026-08-24T13:45:02Z, cross-checked against Python's datetime.
        CHECK_EQ(e.fix().time.toUnixSeconds(), 1787579102ull);
        feed(e, kRmcNoFix);
        CHECK_EQ(e.fix().time.toUnixSeconds(), 1787579103ull);
    }
    {
        GnssTime epoch;
        epoch.year = 1970; epoch.month = 1; epoch.day = 1; epoch.valid = true;
        CHECK_EQ(epoch.toUnixSeconds(), 0ull);
        GnssTime y2k;
        y2k.year = 2000; y2k.month = 1; y2k.day = 1; y2k.valid = true;
        CHECK_EQ(y2k.toUnixSeconds(), 946684800ull);
        // A leap day must not shift the following date by a day.
        GnssTime leap;
        leap.year = 2024; leap.month = 3; leap.day = 1; leap.valid = true;
        CHECK_EQ(leap.toUnixSeconds(), 1709251200ull);
        // An invalid time yields 0 rather than a plausible-looking wrong answer.
        GnssTime unset;
        CHECK_EQ(unset.toUnixSeconds(), 0ull);
    }

    // takeUpdated is edge-triggered.
    NmeaParser u;
    feed(u, kRmc);
    CHECK_TRUE(u.takeUpdated());
    CHECK_FALSE(u.takeUpdated());

    // --- the per-satellite table -------------------------------------------
    {
        NmeaParser sky;
        feed(sky, kGpGsv1);
        // A GSV cycle is published whole or not at all: half a sky is worse to
        // draw than a slightly stale one.
        CHECK_EQ(static_cast<long long>(sky.satelliteCount()), 0ll);
        feed(sky, kGpGsv2);
        CHECK_EQ(static_cast<long long>(sky.satelliteCount()), 7ll);

        feed(sky, kGlGsv);
        CHECK_EQ(static_cast<long long>(sky.satelliteCount()), 11ll);

        // Elevation, azimuth and carrier-to-noise come through per satellite.
        const SatelliteView* prn4 = nullptr;
        for (size_t i = 0; i < sky.satelliteCount(); ++i) {
            if (sky.satellite(i).prn == 4) prn4 = &sky.satellite(i);
        }
        CHECK_TRUE(prn4 != nullptr);
        if (prn4 != nullptr) {
            CHECK_EQ(prn4->elevation, 55);
            CHECK_EQ(prn4->azimuth, 120);
            CHECK_EQ(prn4->cn0, 42);
            CHECK_STREQ(prn4->talker, "GP");
            // Nothing has said which satellites the solution used yet.
            CHECK_FALSE(prn4->usedInFix);
        }

        // GSA is what marks them used. Its PRNs are 04 07 08 09 16 21 26 27.
        feed(sky, kGsa);
        int used = 0, glonassUsed = 0;
        for (size_t i = 0; i < sky.satelliteCount(); ++i) {
            const SatelliteView& s = sky.satellite(i);
            if (!s.usedInFix) continue;
            ++used;
            if (std::strncmp(s.talker, "GL", 2) == 0) ++glonassUsed;
        }
        // Seven of the eight are in view; PRN 27 is used but not reported by
        // GSV, and must not invent a row.
        CHECK_EQ(used, 7);
        // No GLONASS satellite is in that list, so none may be marked.
        CHECK_EQ(glonassUsed, 0);

        // A second GSA run replaces the first rather than accumulating. The
        // run is delimited by any other sentence type, which is what separates
        // one cycle's GSAs from the next: within a cycle they arrive together.
        feed(sky, kRmc);
        feed(sky, "$GNGSA,A,3,04,,,,,,,,,,,,1.72,0.94,1.44*10");
        int usedAfter = 0;
        for (size_t i = 0; i < sky.satelliteCount(); ++i) {
            if (sky.satellite(i).usedInFix) ++usedAfter;
        }
        CHECK_EQ(usedAfter, 1);

        // A fresh GSV cycle replaces the constellation's rows, and does not
        // disturb the other constellation's.
        feed(sky, "$GPGSV,1,1,02,04,10,020,20,07,20,030,25*7F");
        CHECK_EQ(static_cast<long long>(sky.satelliteCount()), 6ll);   // 2 GPS + 4 GLONASS
    }

    // --- satellites in view but not tracked --------------------------------
    {
        // An acquiring receiver reports the satellite with every field empty.
        // It belongs on the plot, dimmed, not dropped.
        NmeaParser acq;
        feed(acq, "$GPGSV,1,1,02,11,,,,12,45,180,33*40");
        CHECK_EQ(static_cast<long long>(acq.satelliteCount()), 2ll);
        CHECK_EQ(acq.satellite(0).prn, 11);
        CHECK_EQ(acq.satellite(0).cn0, 0);
        CHECK_EQ(acq.satellite(0).elevation, 0);
        CHECK_EQ(acq.satellite(1).cn0, 33);
    }

    // --- constellation names ------------------------------------------------
    {
        CHECK_STREQ(constellationName("GP"), "GPS");
        CHECK_STREQ(constellationName("GL"), "GLONASS");
        CHECK_STREQ(constellationName("GA"), "Galileo");
        CHECK_STREQ(constellationName("GB"), "BeiDou");
        CHECK_STREQ(constellationName("GQ"), "QZSS");
        CHECK_STREQ(constellationName("ZZ"), "?");
        CHECK_STREQ(constellationName(nullptr), "?");
    }

    return check::finish("nmea");
}
