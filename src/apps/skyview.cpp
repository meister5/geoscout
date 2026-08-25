#include "skyview.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../config.h"
#include "../shell/theme.h"

namespace geoscout {
namespace {

// The polar plot fills the left of the screen; the carrier-to-noise list fills
// the right.
constexpr int kPlotCentreX = 60;
constexpr int kPlotCentreY = kStatusBarHeight + (kScreenHeight - kStatusBarHeight) / 2;
constexpr int kPlotRadius = 54;

constexpr int kPanelX = 122;
constexpr int kPanelRows = 10;
constexpr int kRowHeight = 10;
constexpr int kBarX = 148;
constexpr int kBarMaxWidth = 62;

// Carrier-to-noise above this is a strong satellite; the bar is scaled so that
// the difference between 20 and 40 dB-Hz is the visible part of the range.
constexpr int kCn0Full = 50;

constexpr double kPi = 3.14159265358979323846;

uint16_t constellationColour(const char* talker) {
    if (std::strncmp(talker, "GP", 2) == 0) return theme::kGps;
    if (std::strncmp(talker, "GL", 2) == 0) return theme::kGlonass;
    if (std::strncmp(talker, "GA", 2) == 0) return theme::kGalileo;
    if (std::strncmp(talker, "GB", 2) == 0 || std::strncmp(talker, "BD", 2) == 0) {
        return theme::kBeidou;
    }
    if (std::strncmp(talker, "GQ", 2) == 0) return theme::kQzss;
    return theme::kOtherSat;
}

// A one-letter prefix so a PRN reads unambiguously in a 30-pixel column.
char constellationInitial(const char* talker) {
    if (std::strncmp(talker, "GP", 2) == 0) return 'G';
    if (std::strncmp(talker, "GL", 2) == 0) return 'R';   // GLONASS, per RINEX
    if (std::strncmp(talker, "GA", 2) == 0) return 'E';   // Galileo, per RINEX
    if (std::strncmp(talker, "GB", 2) == 0 || std::strncmp(talker, "BD", 2) == 0) return 'C';
    if (std::strncmp(talker, "GQ", 2) == 0) return 'J';
    return '?';
}

void polarToScreen(uint16_t azimuthDeg, uint8_t elevationDeg, int* outX, int* outY) {
    // Zenith at the centre, horizon at the rim, north up and east to the right
    // -- the same way every sky plot has been drawn since paper ones.
    const double radius = kPlotRadius * (90.0 - static_cast<double>(elevationDeg)) / 90.0;
    const double azimuth = static_cast<double>(azimuthDeg) * kPi / 180.0;
    *outX = kPlotCentreX + static_cast<int>(std::lround(radius * std::sin(azimuth)));
    *outY = kPlotCentreY - static_cast<int>(std::lround(radius * std::cos(azimuth)));
}

}  // namespace

void SkyViewApp::onEnter(Services& services) {
    parser_ = services.parser;
}

void SkyViewApp::menuLine(const Frame& frame, char* out, size_t outLen) const {
    const GnssFix& fix = *frame.fix;
    if (fix.satsInView == 0) {
        std::snprintf(out, outLen, "no satellites reported yet");
        return;
    }
    std::snprintf(out, outLen, "%u used of %u, best %u dB-Hz",
                  static_cast<unsigned>(fix.satsUsed),
                  static_cast<unsigned>(fix.satsInView),
                  static_cast<unsigned>(fix.maxCn0));
}

void SkyViewApp::draw(M5Canvas& canvas, const Frame& frame) {
    drawPlot(canvas);
    drawSatellites(canvas);
    drawPanel(canvas, frame);
}

void SkyViewApp::drawPlot(M5Canvas& canvas) const {
    canvas.drawCircle(kPlotCentreX, kPlotCentreY, kPlotRadius, theme::kTextFaint);
    // Thirty and sixty degrees of elevation.
    canvas.drawCircle(kPlotCentreX, kPlotCentreY, kPlotRadius * 2 / 3, theme::kPanelHi);
    canvas.drawCircle(kPlotCentreX, kPlotCentreY, kPlotRadius / 3, theme::kPanelHi);

    canvas.drawFastHLine(kPlotCentreX - kPlotRadius, kPlotCentreY, kPlotRadius * 2,
                         theme::kPanelHi);
    canvas.drawFastVLine(kPlotCentreX, kPlotCentreY - kPlotRadius, kPlotRadius * 2,
                         theme::kPanelHi);

    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(theme::kTextDim);
    canvas.setCursor(kPlotCentreX - 2, kPlotCentreY - kPlotRadius - 1);
    canvas.print("N");
    canvas.setCursor(kPlotCentreX + kPlotRadius - 4, kPlotCentreY - 3);
    canvas.print("E");
    canvas.setCursor(kPlotCentreX - 2, kPlotCentreY + kPlotRadius - 7);
    canvas.print("S");
    canvas.setCursor(kPlotCentreX - kPlotRadius + 1, kPlotCentreY - 3);
    canvas.print("W");
}

void SkyViewApp::drawSatellites(M5Canvas& canvas) const {
    if (parser_ == nullptr) return;

    for (size_t i = 0; i < parser_->satelliteCount(); ++i) {
        const SatelliteView& sat = parser_->satellite(i);

        // A satellite reported in view but not yet tracked has no elevation or
        // azimuth to plot. Drawing it at the origin would put it at the zenith,
        // which is a lie; it belongs in the list on the right instead.
        if (sat.elevation == 0 && sat.azimuth == 0 && sat.cn0 == 0) continue;

        int x = 0;
        int y = 0;
        polarToScreen(sat.azimuth, sat.elevation, &x, &y);
        const uint16_t colour = constellationColour(sat.talker);

        if (sat.usedInFix) {
            // Filled means the solution is actually standing on it.
            canvas.fillCircle(x, y, 3, colour);
        } else {
            canvas.drawCircle(x, y, 3, colour);
        }
    }
}

void SkyViewApp::drawPanel(M5Canvas& canvas, const Frame& frame) const {
    canvas.drawFastVLine(kPanelX - 6, kStatusBarHeight + 2, kScreenHeight - kStatusBarHeight - 4,
                         theme::kPanelHi);

    const GnssFix& fix = *frame.fix;
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(theme::kAccent);
    canvas.setCursor(kPanelX, kStatusBarHeight + 4);
    canvas.printf("%u used / %u seen", static_cast<unsigned>(fix.satsUsed),
                  static_cast<unsigned>(fix.satsInView));

    if (parser_ == nullptr || parser_->satelliteCount() == 0) {
        canvas.setTextColor(theme::kTextFaint);
        canvas.setCursor(kPanelX, kStatusBarHeight + 20);
        canvas.print("waiting for GSV");
        return;
    }

    // Strongest first. An insertion sort over an index array: the list is at
    // most forty-eight long and is already nearly sorted between frames.
    const size_t count = parser_->satelliteCount();
    uint8_t order[NmeaParser::kMaxSatellites];
    for (size_t i = 0; i < count; ++i) order[i] = static_cast<uint8_t>(i);
    for (size_t i = 1; i < count; ++i) {
        const uint8_t key = order[i];
        const uint8_t keyCn0 = parser_->satellite(key).cn0;
        size_t j = i;
        while (j > 0 && parser_->satellite(order[j - 1]).cn0 < keyCn0) {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = key;
    }

    const size_t shown = count < kPanelRows ? count : kPanelRows;
    for (size_t row = 0; row < shown; ++row) {
        const SatelliteView& sat = parser_->satellite(order[row]);
        const int y = kStatusBarHeight + 18 + static_cast<int>(row) * kRowHeight;
        const uint16_t colour = constellationColour(sat.talker);

        canvas.setTextColor(sat.usedInFix ? colour : theme::kTextFaint);
        canvas.setCursor(kPanelX, y);
        canvas.printf("%c%02u", constellationInitial(sat.talker),
                      static_cast<unsigned>(sat.prn % 100));

        if (sat.cn0 > 0) {
            int width = sat.cn0 * kBarMaxWidth / kCn0Full;
            if (width > kBarMaxWidth) width = kBarMaxWidth;
            canvas.fillRect(kBarX, y + 1, width, 5,
                            sat.usedInFix ? colour : theme::kPanelHi);
            canvas.setTextColor(theme::kTextDim);
            canvas.setCursor(kBarX + kBarMaxWidth + 4, y);
            canvas.printf("%u", static_cast<unsigned>(sat.cn0));
        } else {
            canvas.setTextColor(theme::kTextFaint);
            canvas.setCursor(kBarX, y);
            canvas.print("acquiring");
        }
    }

    if (count > shown) {
        canvas.setTextColor(theme::kTextFaint);
        canvas.setCursor(kPanelX, kStatusBarHeight + 18 + kPanelRows * kRowHeight);
        canvas.printf("+%u more", static_cast<unsigned>(count - shown));
    }
}

}  // namespace geoscout
