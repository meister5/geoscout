#include "display.h"

#include <Arduino.h>
#include <M5Unified.h>

#include "../config.h"

namespace geoscout {
namespace hal {

bool Display::begin(uint8_t brightness) {
    M5.Display.setRotation(1);
    M5.Display.setBrightness(brightness);
    M5.Display.fillScreen(TFT_BLACK);

    canvas_.setPsram(false);   // there is none; asking wastes a failed attempt
    canvas_.setColorDepth(16);
    ready_ = canvas_.createSprite(kScreenWidth, kScreenHeight) != nullptr;
    return ready_;
}

void Display::present() {
    if (!ready_) return;
    canvas_.pushSprite(&M5.Display, 0, 0);
}

void Display::setBrightness(uint8_t brightness) {
    M5.Display.setBrightness(brightness < 16 ? 16 : brightness);
}

uint32_t Display::freeHeapBytes() {
    return static_cast<uint32_t>(ESP.getFreeHeap());
}

}  // namespace hal
}  // namespace geoscout
