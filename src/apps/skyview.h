// Sky View -- every satellite the receiver can see, where it is, and how well
// it is being heard.
#pragma once

#include "../shell/app.h"

namespace geoscout {

class SkyViewApp : public App {
public:
    const char* name() const override { return "Sky View"; }
    uint8_t desiredFps() const override { return 5; }

    void menuLine(const Frame& frame, char* out, size_t outLen) const override;
    void onEnter(Services& services) override;
    void draw(M5Canvas& canvas, const Frame& frame) override;

private:
    void drawPlot(M5Canvas& canvas) const;
    void drawSatellites(M5Canvas& canvas) const;
    void drawPanel(M5Canvas& canvas, const Frame& frame) const;

    const NmeaParser* parser_ = nullptr;
};

}  // namespace geoscout
