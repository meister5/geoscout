// geoscout -- a GNSS pocket atlas for the M5Stack Cardputer ADV.
//
// Boot order matters here. The display comes up first so that a failure after
// it has something to be reported on; the keyboard needs an explicit begin()
// that M5.begin() does not perform on the ADV; and the canvas is the largest
// allocation in the firmware, so if it is going to fail it fails before
// anything else has taken heap.

#include <Arduino.h>
#include <M5Unified.h>

#include "apps/about.h"
#include "apps/globe.h"
#include "apps/position.h"
#include "apps/settingsapp.h"
#include "apps/skyview.h"
#include "config.h"
#include "hal/display.h"
#include "hal/gnss.h"
#include "hal/keys.h"
#include "shell/settings.h"
#include "shell/shell.h"
#include "shell/theme.h"

namespace {

geoscout::Settings g_settings;
geoscout::hal::Display g_display;
geoscout::hal::Keys g_keys;
geoscout::hal::Gnss g_gnss;
geoscout::Shell g_shell;

// Statically allocated, in menu order. There are five of them and they live for
// the life of the firmware; a heap allocation here would buy nothing.
geoscout::PositionApp g_position;
geoscout::GlobeApp g_globe;
geoscout::SkyViewApp g_skyView;
geoscout::SettingsApp g_settingsApp;
geoscout::AboutApp g_about;

// A fatal error has to be readable on the panel. There is no console attached
// to this thing in the field.
[[noreturn]] void halt(const char* message) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(geoscout::theme::kBad);
    M5.Display.setCursor(8, 40);
    M5.Display.print(message);
    for (;;) delay(1000);
}

void splash() {
    M5.Display.fillScreen(geoscout::theme::kBackground);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextColor(geoscout::theme::kAccent);
    M5.Display.setCursor(30, 44);
    M5.Display.print(geoscout::kAppName);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextColor(geoscout::theme::kTextFaint);
    M5.Display.setCursor(30, 76);
    M5.Display.printf("v%s   waiting for satellites", geoscout::kAppVersion);
    delay(900);
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = false;
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    M5.begin(cfg);

    g_settings.load();

    if (!g_display.begin(g_settings.brightness)) {
        halt("out of memory: canvas");
    }
    splash();

    // Refuses on anything that is not an ADV rather than driving the internal
    // I2C bus as a key matrix, which is what the original Cardputer's reader
    // would do to the pins the codec and the Cap sit on.
    if (!g_keys.begin()) {
        halt("Cardputer ADV required");
    }

    g_gnss.begin();

    g_shell.begin(&g_display, &g_keys, &g_gnss, &g_settings);
    g_shell.addApp(&g_position);
    g_shell.addApp(&g_globe);
    g_shell.addApp(&g_skyView);
    g_shell.addApp(&g_settingsApp);
    g_shell.addApp(&g_about);
}

void loop() {
    g_shell.tick();
}
