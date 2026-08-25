#include "position.h"

#include <cstdio>
#include <cstring>

#include "../config.h"
#include "../shell/theme.h"

namespace geoscout {
namespace {

// The axis formats print latitude and longitude on their own lines; the grid
// formats describe the position as one string. Which of the two a format is
// decides the whole layout, so it is asked once, here.
bool isGridFormat(CoordFormat f) {
    return f == CoordFormat::Maidenhead || f == CoordFormat::Utm || f == CoordFormat::Mgrs;
}

void formatAxis(CoordFormat f, double deg, bool isLatitude, char* out, size_t outLen) {
    switch (f) {
        case CoordFormat::DegMinSec:     formatDms(deg, isLatitude, out, outLen); break;
        case CoordFormat::DegDecimalMin: formatDdm(deg, isLatitude, out, outLen); break;
        default:                         formatDecimal(deg, isLatitude, out, outLen); break;
    }
}

void formatGrid(CoordFormat f, const Coord& c, char* out, size_t outLen) {
    switch (f) {
        case CoordFormat::Maidenhead: formatMaidenhead(c, 8, out, outLen); break;
        case CoordFormat::Utm:        formatUtm(c, out, outLen); break;
        default:                      formatMgrs(c, out, outLen); break;
    }
}

}  // namespace

void PositionApp::onEnter(Services& services) {
    settings_ = services.settings;
}

void PositionApp::menuLine(const Frame& frame, char* out, size_t outLen) const {
    if (!frame.hasFix() || !coordValid(frame.fix->coord)) {
        std::snprintf(out, outLen, "no fix - %u in view",
                      static_cast<unsigned>(frame.fix->satsInView));
        return;
    }
    std::snprintf(out, outLen, "%.5f, %.5f",
                  frame.fix->coord.latDeg(), frame.fix->coord.lonDeg());
}

void PositionApp::cycleFormat(int delta) {
    if (settings_ == nullptr) return;
    const int count = static_cast<int>(CoordFormat::Count);
    int next = static_cast<int>(settings_->coordFormat) + delta;
    while (next < 0) next += count;
    settings_->coordFormat = static_cast<CoordFormat>(next % count);
}

bool PositionApp::onKey(hal::Key key, char ch) {
    (void)ch;
    switch (key) {
        case hal::Key::Left:  cycleFormat(-1); return true;
        case hal::Key::Right:
        case hal::Key::Enter: cycleFormat(1);  return true;
        default:              return false;
    }
}

void PositionApp::draw(M5Canvas& canvas, const Frame& frame) {
    if (!frame.hasFix() || !coordValid(frame.fix->coord)) {
        drawAcquiring(canvas, frame);
        return;
    }
    drawFix(canvas, frame);
}

void PositionApp::drawAcquiring(M5Canvas& canvas, const Frame& frame) {
    const GnssFix& fix = *frame.fix;

    canvas.setFont(&fonts::Font4);
    canvas.setTextColor(frame.everHadFix ? theme::kWarn : theme::kTextDim);
    canvas.setCursor(12, 34);
    canvas.print(frame.everHadFix ? "FIX LOST" : "ACQUIRING");

    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(theme::kTextDim);
    canvas.setCursor(12, 68);
    canvas.printf("%u satellites in view", static_cast<unsigned>(fix.satsInView));

    canvas.setCursor(12, 82);
    if (fix.maxCn0 > 0) {
        // Carrier-to-noise is the honest progress bar during a cold start: the
        // count climbs long before the solution does.
        canvas.printf("strongest %u dB-Hz, mean %.0f",
                      static_cast<unsigned>(fix.maxCn0), fix.meanCn0);
    } else {
        canvas.print("no carrier yet");
    }

    canvas.setCursor(12, 96);
    canvas.printf("%lu s since power on", static_cast<unsigned long>(frame.nowMs / 1000));

    canvas.setTextColor(theme::kTextFaint);
    canvas.setCursor(12, 118);
    canvas.print("cold start takes ~25 s under open sky");
}

void PositionApp::drawFix(M5Canvas& canvas, const Frame& frame) {
    const GnssFix& fix = *frame.fix;
    const CoordFormat format =
        settings_ != nullptr ? settings_->coordFormat : CoordFormat::DecimalDegrees;
    char buf[40];

    // Format name, with the keys that change it.
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(theme::kAccent);
    canvas.setCursor(6, 16);
    canvas.print(coordFormatName(format));
    canvas.setTextColor(theme::kTextFaint);
    canvas.setCursor(kScreenWidth - 60, 16);
    canvas.print(", / cycle");

    canvas.setTextColor(theme::kText);
    if (isGridFormat(format)) {
        formatGrid(format, fix.coord, buf, sizeof(buf));
        // Maidenhead is short enough for the large face; UTM and MGRS are not.
        canvas.setFont(format == CoordFormat::Maidenhead ? &fonts::Font4 : &fonts::Font2);
        canvas.setCursor(6, 34);
        canvas.print(buf);

        // The underlying degrees stay on screen: a grid reference you cannot
        // sanity-check is a grid reference you cannot trust.
        canvas.setFont(&fonts::Font0);
        canvas.setTextColor(theme::kTextDim);
        canvas.setCursor(6, 62);
        canvas.printf("%.6f  %.6f", fix.coord.latDeg(), fix.coord.lonDeg());
    } else {
        canvas.setFont(&fonts::Font4);
        formatAxis(format, fix.coord.latDeg(), true, buf, sizeof(buf));
        canvas.setCursor(6, 28);
        canvas.print(buf);
        formatAxis(format, fix.coord.lonDeg(), false, buf, sizeof(buf));
        canvas.setCursor(6, 54);
        canvas.print(buf);
    }

    canvas.drawFastHLine(0, 84, kScreenWidth, theme::kPanelHi);

    // Two columns of the numbers that qualify the position.
    const Settings defaults;
    const Settings& settings = settings_ != nullptr ? *settings_ : defaults;

    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(theme::kTextDim);

    canvas.setCursor(6, 90);
    canvas.printf("ALT %.0f %s", settings.altitudeFromMeters(fix.altitudeM),
                  settings.altitudeUnit());
    canvas.setCursor(6, 102);
    canvas.printf("SPD %.1f %s", settings.speedFromKph(fix.speedKph), settings.speedUnit());
    canvas.setCursor(6, 114);
    if (fix.speedKph > 1.0) {
        // Course over ground is meaningless when standing still: the receiver
        // reports the noise, and it spins.
        canvas.printf("CRS %.0f %s", fix.courseDeg, compassPoint(fix.courseDeg));
    } else {
        canvas.print("CRS --");
    }

    canvas.setCursor(126, 90);
    canvas.printf("SAT %u/%u", static_cast<unsigned>(fix.satsUsed),
                  static_cast<unsigned>(fix.satsInView));
    canvas.setCursor(126, 102);
    canvas.printf("PDOP %.2f", fix.pdop);
    canvas.setCursor(126, 114);
    if (frame.fixAgeMs == UINT32_MAX) {
        canvas.print("AGE --");
    } else {
        // Age is what separates a position from a memory of one.
        canvas.setTextColor(frame.fixAgeMs > 3000 ? theme::kWarn : theme::kTextDim);
        canvas.printf("AGE %.1f s", static_cast<double>(frame.fixAgeMs) / 1000.0);
    }
}

}  // namespace geoscout
