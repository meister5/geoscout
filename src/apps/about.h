// About -- what this is, what it is running on, and where the map came from.
//
// The attribution is not decoration: the country outlines are Natural Earth,
// and saying so on the device is the polite half of using public domain data.
#pragma once

#include "../shell/app.h"

namespace geoscout {

class AboutApp : public App {
public:
    const char* name() const override { return "About"; }
    uint8_t desiredFps() const override { return 2; }

    void menuLine(const Frame& frame, char* out, size_t outLen) const override;
    void draw(M5Canvas& canvas, const Frame& frame) override;
};

}  // namespace geoscout
