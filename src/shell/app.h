// What an app is.
//
// The shell owns the hardware and the frame loop; an app owns a screen and the
// keys pressed while it is on it. Nothing in an app touches the GNSS UART, the
// display driver or the preferences store directly -- it is handed a fix, a
// canvas and a settings reference, which is what lets two apps be live at once
// and what keeps app number twelve from having to know the receiver exists.
#pragma once

#include <cstddef>
#include <cstdint>

#include <M5GFX.h>

#include "../hal/display.h"
#include "../hal/keys.h"
#include "settings.h"

#include "nmea.h"
#include "solar.h"

namespace geoscout {

// Everything an app is given about this instant. Rebuilt by the shell each
// frame and passed by const reference; apps never cache the fix, because the
// next frame's may be older (the receiver can lose lock) and a stale copy shown
// as live is the one failure mode that matters here.
struct Frame {
    uint32_t nowMs = 0;
    uint32_t dtMs = 0;

    const GnssFix* fix = nullptr;
    uint32_t fixAgeMs = 0;       // UINT32_MAX when there has never been one
    bool everHadFix = false;

    // Zero when the receiver has not yet reported a date. Everything derived
    // from the clock -- the terminator above all -- must check this rather than
    // quietly drawing 1970.
    uint64_t unixSeconds = 0;
    SubsolarPoint sun;

    bool hasFix() const { return fix != nullptr && fix->valid; }
};

// Long-lived things an app may reach for. Held by reference: the shell outlives
// every app.
struct Services {
    Settings* settings = nullptr;
    const NmeaParser* parser = nullptr;   // for the per-satellite table
    // Only the settings screen should reach for this, and only to apply a
    // brightness change the moment it is made rather than on the way out.
    hal::Display* display = nullptr;
};

class App {
public:
    virtual ~App() = default;

    // Shown in the launcher and in the status bar.
    virtual const char* name() const = 0;

    // One line of live detail under the name in the launcher, which is what
    // turns the menu into a dashboard. Write nothing to leave the row blank.
    virtual void menuLine(const Frame& frame, char* out, size_t outLen) const {
        (void)frame;
        if (outLen > 0) out[0] = '\0';
    }

    // How often this app wants to be redrawn. The globe wants every frame; a
    // page of text does not, and asking for five saves the battery that would
    // otherwise go into redrawing static digits thirty times a second.
    virtual uint8_t desiredFps() const { return 5; }

    virtual void onEnter(Services& services) { (void)services; }
    virtual void onExit() {}

    // Return true to keep the key; return false to let the shell handle it,
    // which is how Back gets you to the menu from anywhere.
    virtual bool onKey(hal::Key key, char ch) {
        (void)key;
        (void)ch;
        return false;
    }

    virtual void update(const Frame& frame) { (void)frame; }

    // The canvas is the full screen. The shell draws the status bar over the
    // top of it afterwards, so an app that wants the whole height can have it
    // and simply keep nothing important in the top twelve pixels.
    virtual void draw(M5Canvas& canvas, const Frame& frame) = 0;
};

}  // namespace geoscout
