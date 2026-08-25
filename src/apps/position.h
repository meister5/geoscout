// Global Position -- where you are, in whichever notation you need it.
#pragma once

#include "../shell/app.h"

namespace geoscout {

class PositionApp : public App {
public:
    const char* name() const override { return "Global Position"; }
    uint8_t desiredFps() const override { return 5; }

    void menuLine(const Frame& frame, char* out, size_t outLen) const override;
    void onEnter(Services& services) override;
    bool onKey(hal::Key key, char ch) override;
    void draw(M5Canvas& canvas, const Frame& frame) override;

private:
    void cycleFormat(int delta);
    void drawAcquiring(M5Canvas& canvas, const Frame& frame);
    void drawFix(M5Canvas& canvas, const Frame& frame);

    Settings* settings_ = nullptr;
};

}  // namespace geoscout
