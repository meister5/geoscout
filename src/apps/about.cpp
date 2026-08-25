#include "about.h"

#include <cstdio>

#include "../config.h"
#include "../hal/display.h"
#include "../shell/theme.h"

#include "worlddata.h"

namespace geoscout {

void AboutApp::menuLine(const Frame& frame, char* out, size_t outLen) const {
    (void)frame;
    std::snprintf(out, outLen, "v%s - %d free heap KB", kAppVersion,
                  static_cast<int>(hal::Display::freeHeapBytes() / 1024));
}

void AboutApp::draw(M5Canvas& canvas, const Frame& frame) {
    (void)frame;

    canvas.setFont(&fonts::Font4);
    canvas.setTextColor(theme::kAccent);
    canvas.setCursor(6, kStatusBarHeight + 4);
    canvas.print(kAppName);

    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(theme::kTextDim);
    canvas.setCursor(6, kStatusBarHeight + 32);
    canvas.printf("version %s", kAppVersion);
    canvas.setCursor(6, kStatusBarHeight + 44);
    canvas.print("M5Stack Cardputer ADV + Cap LoRa-1262");
    canvas.setCursor(6, kStatusBarHeight + 56);
    canvas.print("ATGM336H GNSS on UART1");

    canvas.setTextColor(theme::kTextFaint);
    canvas.setCursor(6, kStatusBarHeight + 74);
    canvas.printf("world: Natural Earth 1:110m, public domain");
    canvas.setCursor(6, kStatusBarHeight + 86);
    canvas.printf("%d vertices, %d polylines in flash", kWorldPointCount,
                  kCoastLineCount + kBorderLineCount + kLakeLineCount);
    canvas.setCursor(6, kStatusBarHeight + 98);
    canvas.printf("free heap %lu bytes",
                  static_cast<unsigned long>(hal::Display::freeHeapBytes()));
}

}  // namespace geoscout
