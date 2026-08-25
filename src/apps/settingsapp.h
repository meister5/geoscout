// Settings -- one screen, one list, changes applied as you make them.
#pragma once

#include "../shell/app.h"

namespace geoscout {

class SettingsApp : public App {
public:
    const char* name() const override { return "Settings"; }
    uint8_t desiredFps() const override { return 8; }

    void menuLine(const Frame& frame, char* out, size_t outLen) const override;
    void onEnter(Services& services) override;
    void onExit() override;
    bool onKey(hal::Key key, char ch) override;
    void draw(M5Canvas& canvas, const Frame& frame) override;

private:
    // Every row is one of these. Keeping the list data-driven is what stops
    // this screen from growing a branch per setting.
    enum class Item : uint8_t {
        CoordFormat = 0,
        Units,
        TimeZone,
        Brightness,
        StatusBar,
        GlobeNight,
        GlobeGraticule,
        GlobeBorders,
        GlobeLakes,
        RestoreDefaults,
        Count,
    };

    void adjust(int delta);
    void activate();
    void describe(Item item, char* out, size_t outLen) const;
    static const char* label(Item item);

    Settings* settings_ = nullptr;
    hal::Display* display_ = nullptr;
    size_t selected_ = 0;
    size_t scrollTop_ = 0;
    bool restored_ = false;
};

}  // namespace geoscout
