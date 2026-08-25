#include "settingsapp.h"

#include <cstdio>
#include <cstdlib>

#include "../config.h"
#include "../shell/theme.h"

namespace geoscout {
namespace {

constexpr int kRowHeight = 11;
constexpr int kListTop = kStatusBarHeight + 2;
constexpr int kHintY = kScreenHeight - 9;
constexpr int kVisibleRows = (kHintY - kListTop) / kRowHeight;
constexpr int kValueRight = kScreenWidth - 6;

// Brightness moves in visible steps rather than by one, which would take
// fifteen presses to notice.
constexpr int kBrightnessStep = 16;
constexpr int kBrightnessMin = 16;

void drawRightText(M5Canvas& canvas, const char* text, int right, int y) {
    canvas.setCursor(right - canvas.textWidth(text), y);
    canvas.print(text);
}

}  // namespace

void SettingsApp::onEnter(Services& services) {
    settings_ = services.settings;
    display_ = services.display;
    restored_ = false;
}

void SettingsApp::onExit() {
    if (settings_ != nullptr) settings_->save();
}

void SettingsApp::menuLine(const Frame& frame, char* out, size_t outLen) const {
    (void)frame;
    if (settings_ == nullptr) {
        std::snprintf(out, outLen, "units, format, brightness");
        return;
    }
    std::snprintf(out, outLen, "%s - %s", coordFormatName(settings_->coordFormat),
                  unitsName(settings_->units));
}

const char* SettingsApp::label(Item item) {
    switch (item) {
        case Item::CoordFormat:     return "Coordinates";
        case Item::Units:           return "Units";
        case Item::TimeZone:        return "Time zone";
        case Item::Brightness:      return "Brightness";
        case Item::StatusBar:       return "Status bar";
        case Item::GlobeNight:      return "Globe: night";
        case Item::GlobeGraticule:  return "Globe: grid";
        case Item::GlobeBorders:    return "Globe: borders";
        case Item::GlobeLakes:      return "Globe: lakes";
        case Item::RestoreDefaults: return "Restore defaults";
        default:                    return "?";
    }
}

void SettingsApp::describe(Item item, char* out, size_t outLen) const {
    if (settings_ == nullptr) {
        out[0] = '\0';
        return;
    }
    const Settings& s = *settings_;
    switch (item) {
        case Item::CoordFormat:
            std::snprintf(out, outLen, "%s", coordFormatName(s.coordFormat));
            break;
        case Item::Units:
            std::snprintf(out, outLen, "%s", unitsName(s.units));
            break;
        case Item::TimeZone: {
            const int minutes = static_cast<int>(s.tzOffsetMinutes());
            std::snprintf(out, outLen, "UTC%c%02d:%02d", minutes < 0 ? '-' : '+',
                          std::abs(minutes) / 60, std::abs(minutes) % 60);
            break;
        }
        case Item::Brightness:
            std::snprintf(out, outLen, "%d%%", (s.brightness * 100 + 127) / 255);
            break;
        case Item::StatusBar:
            std::snprintf(out, outLen, "%s", s.showStatusBar ? "on" : "off");
            break;
        case Item::GlobeNight:
            std::snprintf(out, outLen, "%s", s.globeNight ? "on" : "off");
            break;
        case Item::GlobeGraticule:
            std::snprintf(out, outLen, "%s", s.globeGraticule ? "on" : "off");
            break;
        case Item::GlobeBorders:
            std::snprintf(out, outLen, "%s", s.globeBorders ? "on" : "off");
            break;
        case Item::GlobeLakes:
            std::snprintf(out, outLen, "%s", s.globeLakes ? "on" : "off");
            break;
        case Item::RestoreDefaults:
            std::snprintf(out, outLen, "%s", restored_ ? "done" : "press enter");
            break;
        default:
            out[0] = '\0';
            break;
    }
}

void SettingsApp::adjust(int delta) {
    if (settings_ == nullptr || delta == 0) return;
    Settings& s = *settings_;
    restored_ = false;

    switch (static_cast<Item>(selected_)) {
        case Item::CoordFormat: {
            const int count = static_cast<int>(CoordFormat::Count);
            int next = static_cast<int>(s.coordFormat) + delta;
            while (next < 0) next += count;
            s.coordFormat = static_cast<CoordFormat>(next % count);
            break;
        }
        case Item::Units: {
            const int count = static_cast<int>(Units::Count);
            int next = static_cast<int>(s.units) + delta;
            while (next < 0) next += count;
            s.units = static_cast<Units>(next % count);
            break;
        }
        case Item::TimeZone: {
            // Quarter-hour steps, because Kathmandu and the Chatham Islands are
            // real places and rounding them to the hour is just wrong.
            int next = s.tzQuarterHours + delta;
            if (next < -48) next = -48;
            if (next > 56) next = 56;
            s.tzQuarterHours = static_cast<int8_t>(next);
            break;
        }
        case Item::Brightness: {
            int next = static_cast<int>(s.brightness) + delta * kBrightnessStep;
            if (next < kBrightnessMin) next = kBrightnessMin;
            if (next > 255) next = 255;
            s.brightness = static_cast<uint8_t>(next);
            // Applied immediately: a brightness setting you cannot see the
            // effect of is a brightness setting you cannot set.
            if (display_ != nullptr) display_->setBrightness(s.brightness);
            break;
        }
        case Item::StatusBar:      s.showStatusBar = !s.showStatusBar; break;
        case Item::GlobeNight:     s.globeNight = !s.globeNight; break;
        case Item::GlobeGraticule: s.globeGraticule = !s.globeGraticule; break;
        case Item::GlobeBorders:   s.globeBorders = !s.globeBorders; break;
        case Item::GlobeLakes:     s.globeLakes = !s.globeLakes; break;
        default: break;
    }
}

void SettingsApp::activate() {
    if (settings_ == nullptr) return;
    if (static_cast<Item>(selected_) == Item::RestoreDefaults) {
        settings_->restoreDefaults();
        if (display_ != nullptr) display_->setBrightness(settings_->brightness);
        restored_ = true;
        return;
    }
    adjust(1);
}

bool SettingsApp::onKey(hal::Key key, char ch) {
    (void)ch;
    switch (key) {
        case hal::Key::Up:
            if (selected_ > 0) --selected_;
            return true;
        case hal::Key::Down:
            if (selected_ + 1 < static_cast<size_t>(Item::Count)) ++selected_;
            return true;
        case hal::Key::Left:  adjust(-1); return true;
        case hal::Key::Right: adjust(1); return true;
        case hal::Key::Enter:
        case hal::Key::Space: activate(); return true;
        default:              return false;
    }
}

void SettingsApp::draw(M5Canvas& canvas, const Frame& frame) {
    (void)frame;
    const size_t count = static_cast<size_t>(Item::Count);

    if (selected_ < scrollTop_) scrollTop_ = selected_;
    if (selected_ >= scrollTop_ + kVisibleRows) scrollTop_ = selected_ - kVisibleRows + 1;

    canvas.setFont(&fonts::Font0);
    char value[24];

    for (int row = 0; row < kVisibleRows; ++row) {
        const size_t index = scrollTop_ + static_cast<size_t>(row);
        if (index >= count) break;
        const Item item = static_cast<Item>(index);
        const bool isSelected = index == selected_;
        const int y = kListTop + row * kRowHeight;

        if (isSelected) {
            canvas.fillRect(0, y - 1, kScreenWidth, kRowHeight, theme::kPanelHi);
            canvas.fillRect(0, y - 1, 2, kRowHeight, theme::kAccent);
        }

        canvas.setTextColor(isSelected ? theme::kText : theme::kTextDim);
        canvas.setCursor(8, y);
        canvas.print(label(item));

        describe(item, value, sizeof(value));
        canvas.setTextColor(isSelected ? theme::kAccent : theme::kTextFaint);
        drawRightText(canvas, value, kValueRight, y);
    }

    canvas.setTextColor(theme::kTextFaint);
    canvas.setCursor(6, kHintY);
    canvas.print(", / change    enter select    ` back");
}

}  // namespace geoscout
