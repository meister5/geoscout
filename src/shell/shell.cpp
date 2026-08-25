#include "shell.h"

#include <Arduino.h>
#include <M5Unified.h>

#include <cstdio>
#include <cstring>

#include "../config.h"
#include "theme.h"

#include "solar.h"

namespace geoscout {
namespace {

constexpr int kRowHeight = 24;
constexpr uint8_t kMenuFps = 4;

// Where the menu list starts. It tucks under the status bar when the bar is on
// and reclaims those pixels when it is off.
int menuTop(const Settings& settings) {
    return settings.showStatusBar ? kStatusBarHeight : 0;
}

void drawRightText(M5Canvas& canvas, const char* text, int right, int y) {
    const int width = canvas.textWidth(text);
    canvas.setCursor(right - width, y);
    canvas.print(text);
}

}  // namespace

void Shell::begin(hal::Display* display, hal::Keys* keys, hal::Gnss* gnss,
                  Settings* settings) {
    display_ = display;
    keys_ = keys;
    gnss_ = gnss;
    settings_ = settings;
    services_.settings = settings;
    services_.parser = &gnss->parser();
    services_.display = display;
    lastFrameMs_ = millis();
}

bool Shell::addApp(App* app) {
    if (appCount_ >= kMaxApps || app == nullptr) return false;
    apps_[appCount_++] = app;
    return true;
}

Frame Shell::buildFrame(uint32_t nowMs) {
    Frame frame;
    frame.nowMs = nowMs;
    frame.dtMs = nowMs - lastFrameMs_;
    frame.fix = &gnss_->fix();
    frame.fixAgeMs = gnss_->fixAgeMs(nowMs);
    frame.everHadFix = gnss_->everHadFix();
    frame.unixSeconds = frame.fix->time.toUnixSeconds();
    frame.sun = subsolarPoint(frame.unixSeconds);
    return frame;
}

void Shell::tick() {
    const uint32_t now = millis();

    // Input and the receiver are serviced every pass, whatever the draw rate.
    // A five-frame-per-second app must still feel immediate under the thumb.
    gnss_->poll(now);
    const hal::Key key = keys_->poll();
    if (key != hal::Key::None) handleKey(key, keys_->lastChar());

    App* app = currentApp_ == kNoApp ? nullptr : apps_[currentApp_];
    const uint8_t fps = app != nullptr ? app->desiredFps() : kMenuFps;
    const uint32_t interval = fps > 0 ? 1000u / fps : 200u;
    if (now - lastDrawMs_ < interval) {
        delay(1);
        return;
    }

    const Frame frame = buildFrame(now);
    lastFrameMs_ = now;
    lastDrawMs_ = now;

    M5Canvas& canvas = display_->canvas();
    canvas.fillScreen(theme::kBackground);

    if (app != nullptr) {
        app->update(frame);
        app->draw(canvas, frame);
    } else {
        drawMenu(canvas, frame);
    }

    // Last, and over the top: an app that wants the full height can have it, so
    // long as it keeps nothing important in the top twelve pixels.
    if (settings_->showStatusBar) drawStatusBar(canvas, frame);

    display_->present();
}

void Shell::handleKey(hal::Key key, char ch) {
    if (currentApp_ != kNoApp) {
        App* app = apps_[currentApp_];
        if (app->onKey(key, ch)) return;
        // Back is the shell's, unless the app claimed it above -- which is how
        // an app with its own sub-page gets to unwind one level at a time.
        if (key == hal::Key::Back) closeApp();
        return;
    }

    switch (key) {
        case hal::Key::Up:
            if (selected_ > 0) --selected_;
            break;
        case hal::Key::Down:
            if (selected_ + 1 < appCount_) ++selected_;
            break;
        case hal::Key::Enter:
        case hal::Key::Space:
        case hal::Key::Right:
            openApp(selected_);
            break;
        case hal::Key::Char:
            // Number keys jump straight to an app, which is faster than
            // scrolling once you know the order.
            if (ch >= '1' && ch <= '9') {
                const size_t index = static_cast<size_t>(ch - '1');
                if (index < appCount_) {
                    selected_ = index;
                    openApp(index);
                }
            }
            break;
        default:
            break;
    }
}

void Shell::openApp(size_t index) {
    if (index >= appCount_) return;
    currentApp_ = index;
    apps_[index]->onEnter(services_);
    lastDrawMs_ = 0;   // draw the new screen on the very next tick
}

void Shell::closeApp() {
    if (currentApp_ == kNoApp) return;
    apps_[currentApp_]->onExit();
    currentApp_ = kNoApp;
    // Settings may have been changed by any app, not only the settings screen:
    // the globe's toggles are persistent too.
    settings_->save();
    display_->setBrightness(settings_->brightness);
    lastDrawMs_ = 0;
}

void Shell::drawMenu(M5Canvas& canvas, const Frame& frame) {
    const int top = menuTop(*settings_);
    const int visibleRows = (kScreenHeight - top) / kRowHeight;

    // Keep the selection on screen without ever showing a partial row.
    if (selected_ < scrollTop_) scrollTop_ = selected_;
    if (visibleRows > 0 && selected_ >= scrollTop_ + static_cast<size_t>(visibleRows)) {
        scrollTop_ = selected_ - static_cast<size_t>(visibleRows) + 1;
    }

    char line[48];
    for (int row = 0; row < visibleRows; ++row) {
        const size_t index = scrollTop_ + static_cast<size_t>(row);
        if (index >= appCount_) break;
        const bool isSelected = index == selected_;
        const int y = top + row * kRowHeight;

        canvas.fillRect(0, y, kScreenWidth, kRowHeight - 1,
                        isSelected ? theme::kPanelHi : theme::kBackground);
        if (isSelected) canvas.fillRect(0, y, 3, kRowHeight - 1, theme::kAccent);

        canvas.setFont(&fonts::Font0);
        canvas.setTextColor(isSelected ? theme::kAccent : theme::kTextFaint);
        canvas.setCursor(8, y + 5);
        canvas.printf("%d", static_cast<int>(index) + 1);

        canvas.setFont(&fonts::Font2);
        canvas.setTextColor(isSelected ? theme::kText : theme::kTextDim);
        canvas.setCursor(20, y + 1);
        canvas.print(apps_[index]->name());

        // The live subtitle is what makes this a dashboard rather than a list
        // of names: the position, the satellite count and the fix quality are
        // all readable without opening anything.
        line[0] = '\0';
        apps_[index]->menuLine(frame, line, sizeof(line));
        if (line[0] != '\0') {
            canvas.setFont(&fonts::Font0);
            canvas.setTextColor(isSelected ? theme::kAccent : theme::kTextFaint);
            canvas.setCursor(20, y + 16);
            canvas.print(line);
        }
    }

    // A scroll hint, only when there is something off screen.
    if (appCount_ > static_cast<size_t>(visibleRows)) {
        const int barHeight = (kScreenHeight - top) * visibleRows / static_cast<int>(appCount_);
        const int barY = top + (kScreenHeight - top) * static_cast<int>(scrollTop_) /
                                   static_cast<int>(appCount_);
        canvas.fillRect(kScreenWidth - 2, barY, 2, barHeight, theme::kTextFaint);
    }
}

void Shell::drawStatusBar(M5Canvas& canvas, const Frame& frame) {
    canvas.fillRect(0, 0, kScreenWidth, kStatusBarHeight, theme::kPanel);
    canvas.setFont(&fonts::Font0);

    const GnssFix& fix = *frame.fix;
    char buf[24];

    // Fix quality, in the colour that says whether to trust the rest of it.
    uint16_t colour = theme::kBad;
    const char* label = "NO FIX";
    if (fix.valid && fix.fixType == FixType::Fix3D) {
        colour = fix.usable() ? theme::kGood : theme::kWarn;
        label = "3D";
    } else if (fix.valid && fix.fixType == FixType::Fix2D) {
        colour = theme::kWarn;
        label = "2D";
    } else if (frame.everHadFix) {
        colour = theme::kWarn;
        label = "LOST";
    }
    canvas.setTextColor(colour);
    canvas.setCursor(4, 2);
    if (fix.valid) {
        std::snprintf(buf, sizeof(buf), "%s %u/%u", label,
                      static_cast<unsigned>(fix.satsUsed),
                      static_cast<unsigned>(fix.satsInView));
    } else {
        std::snprintf(buf, sizeof(buf), "%s %u", label,
                      static_cast<unsigned>(fix.satsInView));
    }
    canvas.print(buf);

    // Dilution of precision. Without it the satellite count is misleading:
    // eleven satellites in a line is a worse position than five spread out.
    canvas.setTextColor(theme::kTextDim);
    canvas.setCursor(76, 2);
    if (fix.valid && fix.hdop < 99.0) {
        std::snprintf(buf, sizeof(buf), "H%.1f", fix.hdop);
        canvas.print(buf);
    } else {
        canvas.print("H--");
    }

    // UTC, dimmed until the receiver has actually delivered a date. The
    // terminator and everything else time-derived depends on this being real.
    canvas.setCursor(112, 2);
    if (fix.time.valid) {
        canvas.setTextColor(theme::kText);
        std::snprintf(buf, sizeof(buf), "%02u:%02u:%02uZ",
                      static_cast<unsigned>(fix.time.hour),
                      static_cast<unsigned>(fix.time.minute),
                      static_cast<unsigned>(fix.time.second));
    } else {
        canvas.setTextColor(theme::kTextFaint);
        std::snprintf(buf, sizeof(buf), "--:--:--");
    }
    canvas.print(buf);

    const int32_t battery = M5.Power.getBatteryLevel();
    if (battery >= 0) {
        canvas.setTextColor(battery < 20 ? theme::kBad : theme::kTextDim);
        std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(battery));
        drawRightText(canvas, buf, kScreenWidth - 4, 2);
    }
}

}  // namespace geoscout
