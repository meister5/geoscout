// Globe -- the Earth, drawn from flash-resident vectors, with your position on
// it and the day/night terminator where it actually is.
#pragma once

#include "../shell/app.h"

#include "sphere.h"
#include "worlddata.h"

namespace geoscout {

class GlobeApp : public App {
public:
    enum class Mode : uint8_t {
        Follow = 0,   // locked to your own position
        Free,         // you steer
        Spin,         // it turns on its own
        Count,
    };

    const char* name() const override { return "Globe"; }
    // The only app that asks for the full frame rate, and the reason the
    // shell has a per-app rate at all.
    uint8_t desiredFps() const override { return 25; }

    void menuLine(const Frame& frame, char* out, size_t outLen) const override;
    void onEnter(Services& services) override;
    bool onKey(hal::Key key, char ch) override;
    void update(const Frame& frame) override;
    void draw(M5Canvas& canvas, const Frame& frame) override;

private:
    static constexpr int kStarCount = 48;
    static constexpr int kMinRadius = 40;
    static constexpr int kMaxRadius = 190;

    void seedStars();
    void setMode(Mode mode);
    void nudge(double dLat, double dLon);

    void drawStars(M5Canvas& canvas) const;
    void drawOcean(M5Canvas& canvas, const View& view, const Frame& frame) const;
    void drawGraticule(M5Canvas& canvas, const FixedView& view) const;
    void drawWorld(M5Canvas& canvas, const FixedView& view, const int32_t* sunAxis) const;
    void drawLayer(M5Canvas& canvas, const FixedView& view, const int32_t* sunAxis,
                   const GeoPolyline* lines, int lineCount,
                   uint16_t dayColour, uint16_t nightColour) const;
    void drawMarkers(M5Canvas& canvas, const FixedView& view, const Frame& frame) const;
    void drawHud(M5Canvas& canvas, const Frame& frame) const;

    Settings* settings_ = nullptr;
    Mode mode_ = Mode::Follow;
    double viewLat_ = 20.0;
    double viewLon_ = 0.0;
    int radius_ = 62;
    bool showHud_ = true;

    // Screen-space star positions, seeded once so they do not crawl between
    // frames the way a per-frame random field would.
    int16_t starX_[kStarCount] = {};
    int16_t starY_[kStarCount] = {};
    uint8_t starBright_[kStarCount] = {};
};

}  // namespace geoscout
