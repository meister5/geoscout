#include "globe.h"

#include <cmath>
#include <cstdio>

#include "../config.h"
#include "../shell/theme.h"

#include "render.h"


namespace geoscout {
namespace {

// The globe sits below the status bar and centred in what is left.
constexpr int kCentreX = kScreenWidth / 2;
constexpr int kCentreY = kStatusBarHeight + (kScreenHeight - kStatusBarHeight) / 2;

constexpr double kSpinDegPerSecond = 14.0;
constexpr double kNudgeDegrees = 6.0;

const char* modeName(GlobeApp::Mode mode) {
    switch (mode) {
        case GlobeApp::Mode::Follow: return "FOLLOW";
        case GlobeApp::Mode::Free:   return "FREE";
        case GlobeApp::Mode::Spin:   return "SPIN";
        default:                     return "?";
    }
}

double clampLatitude(double lat) {
    if (lat > 90.0) return 90.0;
    if (lat < -90.0) return -90.0;
    return lat;
}

}  // namespace

void GlobeApp::onEnter(Services& services) {
    settings_ = services.settings;
    seedStars();
}

void GlobeApp::seedStars() {
    // A fixed linear congruential sequence rather than random(): the sky should
    // be the same sky every time the app opens, and it must not shimmer.
    uint32_t state = 0x9E3779B9u;
    const auto next = [&state]() {
        state = state * 1664525u + 1013904223u;
        return state >> 16;
    };
    for (int i = 0; i < kStarCount; ++i) {
        starX_[i] = static_cast<int16_t>(next() % kScreenWidth);
        starY_[i] = static_cast<int16_t>(kStatusBarHeight +
                                         next() % (kScreenHeight - kStatusBarHeight));
        starBright_[i] = static_cast<uint8_t>(next() % 4);
    }
}

void GlobeApp::menuLine(const Frame& frame, char* out, size_t outLen) const {
    if (frame.sun.valid) {
        std::snprintf(out, outLen, "%s - sun over %.0f, %.0f", modeName(mode_),
                      frame.sun.latDeg, frame.sun.lonDeg);
    } else {
        std::snprintf(out, outLen, "%s - waiting for UTC", modeName(mode_));
    }
}

void GlobeApp::setMode(Mode mode) { mode_ = mode; }

void GlobeApp::nudge(double dLat, double dLon) {
    // Steering means you have taken the wheel: nothing is more irritating than
    // a view that snaps back to where the firmware wanted it.
    setMode(Mode::Free);
    viewLat_ = clampLatitude(viewLat_ + dLat);
    viewLon_ = wrapLongitude(viewLon_ + dLon);
}

bool GlobeApp::onKey(hal::Key key, char ch) {
    switch (key) {
        case hal::Key::Up:    nudge(kNudgeDegrees, 0.0); return true;
        case hal::Key::Down:  nudge(-kNudgeDegrees, 0.0); return true;
        case hal::Key::Left:  nudge(0.0, -kNudgeDegrees); return true;
        case hal::Key::Right: nudge(0.0, kNudgeDegrees); return true;
        case hal::Key::Enter:
            setMode(static_cast<Mode>((static_cast<int>(mode_) + 1) %
                                      static_cast<int>(Mode::Count)));
            return true;
        case hal::Key::Char:
            break;
        default:
            return false;
    }

    if (settings_ == nullptr) return false;
    switch (ch) {
        case hal::kKeyZoomIn:
            radius_ = radius_ + 12 > kMaxRadius ? kMaxRadius : radius_ + 12;
            return true;
        case hal::kKeyZoomOut:
            radius_ = radius_ - 12 < kMinRadius ? kMinRadius : radius_ - 12;
            return true;
        case 'g': settings_->globeGraticule = !settings_->globeGraticule; return true;
        case 'n': settings_->globeNight = !settings_->globeNight; return true;
        case 'b': settings_->globeBorders = !settings_->globeBorders; return true;
        case 'l': settings_->globeLakes = !settings_->globeLakes; return true;
        case 'h': showHud_ = !showHud_; return true;
        default:  return false;
    }
}

void GlobeApp::update(const Frame& frame) {
    switch (mode_) {
        case Mode::Follow:
            if (frame.hasFix() && coordValid(frame.fix->coord)) {
                viewLat_ = frame.fix->coord.latDeg();
                viewLon_ = frame.fix->coord.lonDeg();
            }
            break;
        case Mode::Spin:
            viewLon_ = wrapLongitude(viewLon_ +
                                     kSpinDegPerSecond * static_cast<double>(frame.dtMs) / 1000.0);
            break;
        default:
            break;
    }
}

void GlobeApp::draw(M5Canvas& canvas, const Frame& frame) {
    const View view = View::centeredOn(viewLat_, viewLon_);
    const FixedView fixedView = FixedView::from(view);

    const Settings defaults;
    const Settings& settings = settings_ != nullptr ? *settings_ : defaults;

    drawStars(canvas);
    drawOcean(canvas, view, frame);
    if (settings.globeGraticule) drawGraticule(canvas, fixedView);

    // The sun direction, in the same fixed-point form as the view basis, so
    // that day and night are one dot product per vertex.
    int32_t sunAxis[3] = {0, 0, 0};
    const bool shade = settings.globeNight && frame.sun.valid;
    if (shade) toFixedAxis(unitFromLatLon(frame.sun.latDeg, frame.sun.lonDeg), sunAxis);

    drawWorld(canvas, fixedView, shade ? sunAxis : nullptr);
    drawMarkers(canvas, fixedView, frame);
    if (showHud_) drawHud(canvas, frame);
}

void GlobeApp::drawStars(M5Canvas& canvas) const {
    for (int i = 0; i < kStarCount; ++i) {
        // Anything inside the disc would be swallowed by the ocean fill a
        // moment later; skipping it saves the overdraw.
        const int dx = starX_[i] - kCentreX;
        const int dy = starY_[i] - kCentreY;
        if (dx * dx + dy * dy < radius_ * radius_) continue;
        canvas.drawPixel(starX_[i], starY_[i],
                         starBright_[i] == 0 ? theme::kStarBright : theme::kStar);
    }
}

namespace {

// Context for the two shared-renderer callbacks. Both are plain function
// pointers so that lib/core stays free of anything M5GFX-shaped.
struct DrawContext {
    M5Canvas* canvas;
    uint16_t dayColour;
    uint16_t nightColour;
};

void emitSegment(void* context, int32_t x0, int32_t y0, int32_t x1, int32_t y1, bool lit) {
    DrawContext* ctx = static_cast<DrawContext*>(context);
    ctx->canvas->drawLine(kCentreX + x0, kCentreY + y0, kCentreX + x1, kCentreY + y1,
                          lit ? ctx->dayColour : ctx->nightColour);
}

void emitSpan(void* context, int32_t x, int32_t y, int32_t width, bool lit) {
    DrawContext* ctx = static_cast<DrawContext*>(context);
    ctx->canvas->drawFastHLine(kCentreX + x, kCentreY + y, width,
                               lit ? ctx->dayColour : ctx->nightColour);
}

}  // namespace

void GlobeApp::drawOcean(M5Canvas& canvas, const View& view, const Frame& frame) const {
    const Settings defaults;
    const Settings& settings = settings_ != nullptr ? *settings_ : defaults;

    if (!settings.globeNight || !frame.sun.valid) {
        canvas.fillCircle(kCentreX, kCentreY, radius_, theme::kOceanDay);
        return;
    }

    DrawContext ctx{&canvas, theme::kOceanDay, theme::kOceanNight};
    renderOcean(view, unitFromLatLon(frame.sun.latDeg, frame.sun.lonDeg), radius_,
                -kCentreY, kScreenHeight - 1 - kCentreY, emitSpan, &ctx);
}

void GlobeApp::drawGraticule(M5Canvas& canvas, const FixedView& view) const {
    int16_t point[3];
    const auto bake = [&point](double lat, double lon) {
        const Vec3 v = unitFromLatLon(lat, lon);
        point[0] = static_cast<int16_t>(std::lround(v.x * 32767.0));
        point[1] = static_cast<int16_t>(std::lround(v.y * 32767.0));
        point[2] = static_cast<int16_t>(std::lround(v.z * 32767.0));
    };

    // Meridians every thirty degrees, then parallels. Ten-degree steps along
    // each line are smooth enough at this size and cheap enough to recompute
    // every frame rather than hold in RAM.
    const auto stroke = [&](bool meridian, double fixedDeg) {
        Screen previous;
        bool havePrevious = false;
        for (int step = -90; step <= 90; step += 10) {
            const int span = meridian ? step : step * 2;
            if (meridian) {
                bake(static_cast<double>(span), fixedDeg);
            } else {
                bake(fixedDeg, static_cast<double>(span));
            }
            const Screen current = project(point, view, radius_);
            if (havePrevious && previous.front && current.front) {
                canvas.drawLine(kCentreX + previous.x, kCentreY + previous.y,
                                kCentreX + current.x, kCentreY + current.y,
                                theme::kGraticule);
            }
            previous = current;
            havePrevious = true;
        }
    };

    for (int lon = -180; lon < 180; lon += 30) stroke(true, static_cast<double>(lon));
    for (int lat = -60; lat <= 60; lat += 30) stroke(false, static_cast<double>(lat));
    stroke(false, 0.0);
}

void GlobeApp::drawLayer(M5Canvas& canvas, const FixedView& view, const int32_t* sunAxis,
                         const GeoPolyline* lines, int lineCount,
                         uint16_t dayColour, uint16_t nightColour) const {
    DrawContext ctx{&canvas, dayColour, nightColour};
    renderLayer(lines, lineCount, view, radius_, sunAxis, emitSegment, &ctx);
}

void GlobeApp::drawWorld(M5Canvas& canvas, const FixedView& view, const int32_t* sunAxis) const {
    const Settings defaults;
    const Settings& settings = settings_ != nullptr ? *settings_ : defaults;

    // Borders first so coastlines draw over them where the two coincide.
    if (settings.globeBorders) {
        drawLayer(canvas, view, sunAxis, kBorderLines, kBorderLineCount,
                  theme::kBorderDay, theme::kBorderNight);
    }
    if (settings.globeLakes) {
        drawLayer(canvas, view, sunAxis, kLakeLines, kLakeLineCount,
                  theme::kLakeDay, theme::kLakeNight);
    }
    drawLayer(canvas, view, sunAxis, kCoastLines, kCoastLineCount,
              theme::kCoastDay, theme::kCoastNight);
}

void GlobeApp::drawMarkers(M5Canvas& canvas, const FixedView& view, const Frame& frame) const {
    int16_t point[3];
    const auto bake = [&point](double lat, double lon) {
        const Vec3 v = unitFromLatLon(lat, lon);
        point[0] = static_cast<int16_t>(std::lround(v.x * 32767.0));
        point[1] = static_cast<int16_t>(std::lround(v.y * 32767.0));
        point[2] = static_cast<int16_t>(std::lround(v.z * 32767.0));
    };

    // The subsolar point, so that the lit half has a visible source.
    if (frame.sun.valid) {
        bake(frame.sun.latDeg, frame.sun.lonDeg);
        const Screen s = project(point, view, radius_);
        if (s.front) {
            canvas.fillCircle(kCentreX + s.x, kCentreY + s.y, 2, theme::kSun);
        }
    }

    if (!frame.hasFix() || !coordValid(frame.fix->coord)) return;

    bake(frame.fix->coord.latDeg(), frame.fix->coord.lonDeg());
    const Screen here = project(point, view, radius_);
    if (!here.front) return;

    const int x = kCentreX + here.x;
    const int y = kCentreY + here.y;

    // A ring that expands and vanishes once a second. It is the one thing on
    // screen that has to be findable at a glance.
    const uint32_t phase = frame.nowMs % 1200;
    if (phase < 800) {
        const int ring = 3 + static_cast<int>(phase / 100);
        canvas.drawCircle(x, y, ring, phase < 400 ? theme::kHere : theme::kBorderDay);
    }
    canvas.fillCircle(x, y, 2, theme::kHere);
}

void GlobeApp::drawHud(M5Canvas& canvas, const Frame& frame) const {
    canvas.setFont(&fonts::Font0);

    canvas.setTextColor(theme::kAccent);
    canvas.setCursor(4, kScreenHeight - 20);
    canvas.print(modeName(mode_));

    canvas.setTextColor(theme::kTextDim);
    canvas.setCursor(4, kScreenHeight - 10);
    canvas.printf("%.1f %c %.1f %c", std::fabs(viewLat_), viewLat_ < 0 ? 'S' : 'N',
                  std::fabs(viewLon_), viewLon_ < 0 ? 'W' : 'E');

    canvas.setTextColor(theme::kTextFaint);
    canvas.setCursor(kScreenWidth - 74, kScreenHeight - 20);
    canvas.printf("zoom %d", radius_);
    canvas.setCursor(kScreenWidth - 74, kScreenHeight - 10);
    if (frame.sun.valid) {
        canvas.print("z x g n b l h");
    } else {
        // Without UTC there is no terminator, and saying so beats drawing a
        // wrong one at longitude zero.
        canvas.setTextColor(theme::kWarn);
        canvas.print("no UTC yet");
    }
}

}  // namespace geoscout
