#include "keys.h"

#include <M5Cardputer.h>
#include <M5Unified.h>

namespace geoscout {
namespace hal {

bool Keys::begin() {
    // The ADV's keyboard is a TCA8418 on the internal I2C bus (G8/G9, INT on
    // G11) and it boots asleep: without this init nothing ever reaches the
    // event FIFO and every poll returns None. M5.begin() does not do it.
    //
    // Keyboard_Class::begin() picks its reader from M5.getBoard(), which M5GFX
    // resolves during M5.begin() by probing G8/G9. If that probe ever comes
    // back as the original Cardputer, the reader it installs drives G8/G9 as
    // 74HC138 outputs -- which on the ADV is the I2C bus the codec, the IMU and
    // the Cap's IO expander all sit on. Refuse rather than wreck the bus.
    if (M5.getBoard() != m5::board_t::board_M5CardputerADV) return false;

    M5Cardputer.Keyboard.begin();
    return true;
}

Key Keys::poll() {
    // Not M5Cardputer.update(): its keyboard half is gated on a private flag
    // that only M5Cardputer.begin() sets, and this firmware calls M5.begin()
    // itself. Drive the two halves directly.
    M5.update();
    M5Cardputer.Keyboard.updateKeyList();
    M5Cardputer.Keyboard.updateKeysState();

    if (!M5Cardputer.Keyboard.isChange()) return Key::None;
    if (!M5Cardputer.Keyboard.isPressed()) return Key::None;

    const Keyboard_Class::KeysState state = M5Cardputer.Keyboard.keysState();
    lastChar_ = 0;

    if (state.enter) return Key::Enter;
    if (state.del) return Key::Back;
    if (state.space) return Key::Space;

    for (const char c : state.word) {
        // The Cardputer has no arrow cluster; the semicolon row doubles as one,
        // which is the convention every Cardputer firmware uses.
        switch (c) {
            case ';': return Key::Up;
            case '.': return Key::Down;
            case ',': return Key::Left;
            case '/': return Key::Right;
            case '`': return Key::Back;
            default: break;
        }
        lastChar_ = c;
        return Key::Char;
    }
    return Key::None;
}

}  // namespace hal
}  // namespace geoscout
