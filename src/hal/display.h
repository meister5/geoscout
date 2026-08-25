// The screen, and the one canvas everything draws into.
//
// A single full-frame sprite is allocated here at boot and shared by every app.
// 240 x 135 at 16 bits is 64,800 bytes, which is the largest single allocation
// this firmware makes and the reason `freeHeapBytes()` is exposed: on a
// Stamp-S3A with no PSRAM there is no second chance if it fails.
#pragma once

#include <cstdint>

#include <M5GFX.h>

namespace geoscout {
namespace hal {

class Display {
public:
    // Returns false if the canvas could not be allocated, which is fatal and
    // which main() reports on the panel rather than hiding in a boot loop.
    bool begin(uint8_t brightness);

    M5Canvas& canvas() { return canvas_; }

    // Pushes the canvas to the panel. One call per frame, after the app and the
    // status bar have both drawn.
    void present();

    void setBrightness(uint8_t brightness);

    static uint32_t freeHeapBytes();

private:
    M5Canvas canvas_;
    bool ready_ = false;
};

}  // namespace hal
}  // namespace geoscout
