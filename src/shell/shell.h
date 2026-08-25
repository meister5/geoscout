// The launcher, the frame loop and the status bar.
//
// The shell owns the hardware. It drains the GNSS receiver every tick, polls
// the keyboard every tick, and hands whichever app is on screen an immutable
// snapshot of the result. Apps come and go; this does not.
#pragma once

#include <cstddef>
#include <cstdint>

#include "../hal/display.h"
#include "../hal/gnss.h"
#include "../hal/keys.h"
#include "app.h"
#include "settings.h"

namespace geoscout {

class Shell {
public:
    static constexpr size_t kMaxApps = 12;

    void begin(hal::Display* display, hal::Keys* keys, hal::Gnss* gnss,
               Settings* settings);

    // Registration order is menu order. Returns false once full.
    bool addApp(App* app);

    // Called from loop() as often as possible. Input is serviced every call;
    // drawing happens at the current app's requested rate.
    void tick();

private:
    void handleKey(hal::Key key, char ch);
    void openApp(size_t index);
    void closeApp();

    Frame buildFrame(uint32_t nowMs);
    void drawMenu(M5Canvas& canvas, const Frame& frame);
    void drawStatusBar(M5Canvas& canvas, const Frame& frame);

    hal::Display* display_ = nullptr;
    hal::Keys* keys_ = nullptr;
    hal::Gnss* gnss_ = nullptr;
    Settings* settings_ = nullptr;
    Services services_;

    App* apps_[kMaxApps] = {};
    size_t appCount_ = 0;

    // kNoApp means the launcher is on screen.
    static constexpr size_t kNoApp = static_cast<size_t>(-1);
    size_t currentApp_ = kNoApp;
    size_t selected_ = 0;
    size_t scrollTop_ = 0;

    uint32_t lastDrawMs_ = 0;
    uint32_t lastFrameMs_ = 0;
};

}  // namespace geoscout
