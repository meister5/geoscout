// NMEA 0183 parser for the ATGM336H.
//
// This is deliberately not TinyGPSPlus. Two reasons: it has to compile on the
// host so fix handling is covered by the native tests, and Sky View needs the
// full GSA/GSV detail -- per-satellite elevation, azimuth and carrier-to-noise,
// across every constellation -- that a fix-only parser throws away. A position
// held on a 2D fix at HDOP 8 is not the same claim as one at HDOP 1, and the
// display has to be able to say so.
#pragma once

#include <cstddef>
#include <cstdint>

#include "geo.h"

namespace geoscout {

enum class FixType : uint8_t {
    None = 1,
    Fix2D = 2,
    Fix3D = 3,
};

struct GnssTime {
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    uint16_t centisecond = 0;
    bool valid = false;

    // ISO-8601 UTC, e.g. "2026-08-24T13:45:02.30Z". Writes at most 25 bytes
    // including the terminator. Returns false and writes an empty string when
    // the time is not yet valid.
    bool toIso8601(char* out, size_t outLen) const;

    // Seconds since the Unix epoch, or 0 when the time is not yet valid. This
    // is the clock the terminator is drawn from: the receiver knows UTC to
    // within a microsecond long before anything else on the device does.
    uint64_t toUnixSeconds() const;
};

// One satellite as GSV reports it, plus whether GSA says the solution actually
// used it. Sky View draws directly from these.
struct SatelliteView {
    char talker[3] = {0, 0, 0};   // "GP", "GL", "GA", "GB", "GQ"
    uint16_t prn = 0;
    uint8_t elevation = 0;        // degrees above the horizon, 0..90
    uint16_t azimuth = 0;         // degrees true, 0..359
    uint8_t cn0 = 0;              // dB-Hz; zero means in view but not tracked
    bool usedInFix = false;
};

// "GPS", "GLONASS", ... for a two-character NMEA talker id. Never null.
const char* constellationName(const char* talker);

struct GnssFix {
    bool valid = false;          // RMC reported an active fix
    FixType fixType = FixType::None;
    Coord coord;
    double altitudeM = 0.0;      // above mean sea level
    double geoidSepM = 0.0;
    double speedKph = 0.0;
    double courseDeg = 0.0;
    uint8_t satsUsed = 0;
    uint8_t satsInView = 0;      // summed across all constellations
    double hdop = 99.99;
    double pdop = 99.99;
    double vdop = 99.99;
    double meanCn0 = 0.0;        // mean C/N0 of satellites reporting one, dB-Hz
    uint8_t maxCn0 = 0;
    GnssTime time;

    // True when the fix is good enough to act on. A position without this is
    // worse than no position at all, because it still looks like one.
    bool usable() const;
};

class NmeaParser {
public:
    static constexpr size_t kMaxSentence = 128;
    static constexpr size_t kMaxTalkers = 8;
    static constexpr size_t kMaxSatsPerTalker = 16;
    static constexpr size_t kMaxSatellites = 48;
    static constexpr size_t kMaxUsedPrns = 32;

    // Feed one character. Returns true when that character completed a sentence
    // that passed its checksum and was understood.
    bool encode(char c);

    const GnssFix& fix() const { return fix_; }

    // True once since the last call if a position-bearing sentence updated the
    // fix. Consuming it clears the flag.
    bool takeUpdated();

    // Every satellite currently in view, across all constellations, flattened
    // out of the per-talker GSV cycles. Ordering is by talker then by the order
    // the receiver listed them, and it is stable between cycles for a given
    // constellation.
    size_t satelliteCount() const { return satCount_; }
    const SatelliteView& satellite(size_t i) const { return sats_[i]; }

    uint32_t sentencesValid() const { return sentencesValid_; }
    uint32_t sentencesUnknown() const { return sentencesUnknown_; }
    uint32_t checksumErrors() const { return checksumErrors_; }
    uint32_t overruns() const { return overruns_; }

    void reset();

private:
    // Each constellation runs its own independent GSV cycle, so in-view counts
    // and carrier-to-noise must accumulate per talker and only then be
    // combined. A single shared accumulator would let whichever constellation
    // reported last overwrite the rest, and the totals would swing with talker
    // order rather than with sky quality.
    struct TalkerState {
        char id[3] = {0, 0, 0};
        uint8_t inView = 0;
        uint32_t cn0Sum = 0;       // published, whole-cycle
        uint16_t cn0Count = 0;
        uint8_t cn0Max = 0;
        uint32_t accumSum = 0;     // in progress
        uint16_t accumCount = 0;
        uint8_t accumMax = 0;

        // Same published/in-progress split for the satellite table: a GSV cycle
        // arrives across several sentences, and half a sky is worse to draw
        // than a slightly stale whole one.
        SatelliteView published[kMaxSatsPerTalker];
        uint8_t publishedCount = 0;
        SatelliteView pending[kMaxSatsPerTalker];
        uint8_t pendingCount = 0;
    };

    bool handleSentence();
    bool handleGga(char* const* f, size_t n);
    bool handleRmc(char* const* f, size_t n);
    bool handleGsa(char* const* f, size_t n);
    bool handleGsv(char* const* f, size_t n, const char* talker);
    TalkerState* talkerSlot(const char* talker);
    void recomputeAggregates();
    void rebuildSatelliteTable();
    bool prnIsUsed(uint16_t prn) const;

    char buf_[kMaxSentence] = {};
    size_t len_ = 0;
    bool inSentence_ = false;

    GnssFix fix_;
    bool updated_ = false;
    uint32_t sentencesValid_ = 0;
    uint32_t sentencesUnknown_ = 0;
    uint32_t checksumErrors_ = 0;
    uint32_t overruns_ = 0;

    TalkerState talkers_[kMaxTalkers];
    size_t talkerCount_ = 0;

    SatelliteView sats_[kMaxSatellites];
    size_t satCount_ = 0;

    // PRNs the solution used, gathered across the run of GSA sentences that
    // opens each cycle. `lastWasGsa_` is what tells one run from the next --
    // the GSAs of a cycle always arrive together.
    uint16_t usedPrns_[kMaxUsedPrns] = {};
    size_t usedPrnCount_ = 0;
    bool lastWasGsa_ = false;
};

// Exposed for testing and for anyone hand-checking a log.
bool nmeaChecksumValid(const char* sentence);
// Converts NMEA ddmm.mmmm plus a hemisphere character into degrees.
bool parseNmeaCoordinate(const char* field, char hemisphere, double* outDegrees);

}  // namespace geoscout
