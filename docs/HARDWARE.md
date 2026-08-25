# Hardware notes — Cardputer ADV + Cap LoRa-1262

geoscout uses two things from this hardware: the **GNSS receiver on the cap**
and the **keyboard and screen on the Cardputer**. The SX1262 radio on the same
cap is left alone — it is not initialised, not powered up and not transmitted
on. Its pin map is recorded at the bottom anyway, because it is the obvious
thing to reach for next.

Source: [M5Stack Cap LoRa-1262 docs](https://docs.m5stack.com/en/cap/Cap_LoRa-1262)
(SKU U214) and the [Cardputer ADV docs](https://docs.m5stack.com/en/core/Cardputer%20ADV).
The cap plugs into the ADV's rear 2×7 Cap-Bus header.

## GNSS — ATGM336H-6N (AT6668 core, UART)

| Item | Value |
|---|---|
| Constellations | GPS, QZSS, BD2, BD3, Galileo, GLONASS |
| Bands | BDS B1I+B1C · GPS/QZSS/SBAS L1 · GAL E1 · GLO R1 |
| Channels | 50 |
| Accuracy | < 1.5 m CEP50 |
| Update rate | up to **10 Hz** |
| Protocol | NMEA 0183 4.1 (also CASIC binary) |
| Default UART | **115200** 8N1 |
| Sensitivity | tracking −162 dBm · acquisition −160 dBm · cold start −148 dBm |
| TTFF | cold 23 s · hot 1 s |
| Antenna | built-in ceramic patch |

Wiring: Cardputer **G13 → GPS-RX**, **G15 ← GPS-TX**, on `HardwareSerial(1)`.

Because it is multi-constellation, Sky View routinely shows satellites from four
systems at once, and the NMEA parser has to key its satellite table on the
*talker* as well as the PRN — `$GPGSV`, `$GLGSV`, `$GAGSV`, `$GBGSV` and
`$GQGSV` all number their satellites from 1 and mean different birds.

### `setRxBufferSize()` goes before `begin()`

The ESP32 Arduino core refuses to resize a UART buffer that is already
allocated — it logs *"RX Buffer can't be resized when Serial is already
running"* and returns 0. Call it after `begin()` and you silently keep the
256-byte default, which at 115200 baud is less than one 10 Hz NMEA cycle. The
symptom is dropped sentences under load, not an error.

## Keyboard — TCA8418 on I²C, and why it needs guarding

This is the single biggest difference from the original Cardputer, and it is not
in the display or the SD slot:

- **Cardputer v1.1** — a scanned key matrix driven through 74HC138 decoders on
  GPIO 8 and 9.
- **Cardputer ADV** — a **TCA8418 keypad controller** on the internal I²C bus
  (**G8 = SDA, G9 = SCL**, INT on G11).

Two consequences:

1. **`M5.begin()` alone never wakes it.** The controller boots asleep, nothing
   reaches its event FIFO, and every poll returns nothing at all. You must call
   `M5Cardputer.Keyboard.begin()`, and then `updateKeyList()` +
   `updateKeysState()` each frame. `M5Cardputer.update()` is not a substitute:
   its keyboard half is gated on a private flag that only
   `M5Cardputer.begin()` sets, and firmware that calls `M5.begin()` itself
   never sets it.

2. **Guard on the board type or you wreck the I²C bus.**
   `Keyboard_Class::begin()` picks its reader from `M5.getBoard()`. If that ever
   comes back as the original Cardputer on ADV hardware, the reader it installs
   drives G8 and G9 as 74HC138 *outputs* — and on the ADV those are the I²C bus
   shared by the audio codec, the IMU and the Cap's IO expander. geoscout
   refuses to boot rather than risk it:

   ```cpp
   if (M5.getBoard() != m5::board_t::board_M5CardputerADV) return false;
   ```

   `main.cpp` turns that into a halt screen reading *"Cardputer ADV required"*.

The **display is not** a difference: both boards carry the same 1.14" 240×135
ST7789, and M5GFX drives it identically.

## Memory — there is no PSRAM

The ADV's Stamp-S3A is an **ESP32-S3FN8**: 8 MB flash, **no PSRAM**, roughly
320 KB of usable SRAM. That shapes the whole renderer:

- The full-frame `M5Canvas` is created with `setPsram(false)` — 240 × 135 × 16bpp
  = **64,800 bytes**, about a fifth of RAM, and the single largest allocation in
  the firmware. `main.cpp` halts with *"out of memory: canvas"* if it fails.
- The world geometry is **`const` in flash**, never copied to RAM: 6,772 int16
  triplets, 42,472 bytes.
- Nothing is allocated per frame.

Do not declare `board_build.psram = enabled` in `platformio.ini` for this board.
It has none, and the resulting image will not boot.

## Power

Idle draw of the whole cap is ~33 mA, essentially all of it the GNSS receiver.
Since geoscout never keys the SX1262, there is no TX burst on top — the radio
sits in reset.

## Pin map

GNSS (UART): **G13 → GPS-RX**, **G15 ← GPS-TX**.

I²C (shared with the HY2.0-4P Grove port and the internal bus): **G8 = SDA**,
**G9 = SCL**.

Cap-Bus header order (left 1–7, right 8–14): `GPS_TX, GPS_RX, SCL, SDA, 5V_OUT,
GND, 5V_IN` | `LoRa_RST, LoRa_IRQ, LoRa_BUSY, LoRa_SCK, LoRa_MOSI, LoRa_MISO,
LoRa_NSS`.

## The radio geoscout does not use

Recorded for whoever adds it. **Semtech SX1262**, 868–923 MHz, +22 dBm max,
−147 dBm sensitivity, on SPI:

| Signal | NSS | MOSI | MISO | SCK | IRQ (DIO1) | RST | BUSY |
|---|---|---|---|---|---|---|---|
| GPIO | **G5** | G14 | G39 | G40 | **G4** | **G3** | **G6** |

Three things bite, and all three are already solved in
[lorascout](https://github.com/meister5/lorascout):

- The **-1262** cap puts an **FM8625H antenna switch** behind a
  **PI4IOE5V6408 I²C IO expander at 0x43**; `P0` is `SX_ANT_SW`. Leave it alone
  and the radio initialises fine and transmits into a dead end.
- **The SPI bus is the microSD bus** — SCK 40, MOSI 14, MISO 39, with separate
  chip selects (LoRa NSS 5, card CS 12). Whichever driver calls `SPI.begin()`
  first decides the pin map for both.
- **Do not call `setTCXO(0)` after RadioLib's `begin()`.** It is not a hint; it
  is a hard chip reset that discards frequency, bandwidth, spreading factor and
  sync word.

Transmitting also brings regulatory duties that reading GPS does not: 868 MHz is
the EU ISM band with duty-cycle limits, 915 MHz is US ISM.
