# Hardware notes — Cardputer ADV + Cap LoRa-1262

geoscout uses two things from this hardware: the **GNSS receiver on the cap**
and the **keyboard and screen on the Cardputer**. The SX1262 radio on the same
cap is left alone — it is not initialised, not powered up and not transmitted
on. Its pin map is recorded at the bottom anyway, because it is the obvious
thing to reach for next.

Every pin, part number and figure below is taken from M5Stack's own
documentation and cross-checked against the source in this repo:
[Cardputer-ADV](https://docs.m5stack.com/en/core/Cardputer-ADV) and
[Cap LoRa-1262](https://docs.m5stack.com/en/cap/Cap_LoRa-1262) (SKU U214).
The cap plugs into the ADV's rear EXT 2.54-14P header.

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

- **Cardputer / Cardputer v1.1** — a scanned key matrix driven through
  **74HC138** decoders on GPIO 8 and 9.
- **Cardputer ADV** — a **TCA8418RTWR** keypad scan controller on the internal
  I²C bus (**G8 = SDA, G9 = SCL, G11 = INT**, address 0x34), scanning a 7×8
  electrical matrix behind the 4×14 / 56-key physical layout.

M5Stack's own comparison table puts it plainly — *Keyboard IO Exp.:*
`Cardputer-Adv → TCA8418RTWR`, `Cardputer v1.1 → 74HC138`,
`Cardputer → 74HC138`.

The trap is that **the ADV and the v1.1 carry the same core module**, the
Stamp-S3A. You cannot tell them apart by the module, which is exactly why
`M5.getBoard()` has to probe, and why trusting it is not optional.

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
   drives G8 and G9 as 74HC138 *outputs* — and on the ADV those two pins are a
   shared I²C bus carrying the **ES8311** audio codec, the **BMI270** IMU, the
   TCA8418 itself, the Grove Port.A and the cap's **PI4IOE5V6408** expander.
   Driving a five-device bus as a pair of decoder outputs is not a bug you
   recover from at runtime, so geoscout refuses to boot instead:

   ```cpp
   if (M5.getBoard() != m5::board_t::board_M5CardputerADV) return false;
   ```

   `main.cpp` turns that into a halt screen reading *"Cardputer ADV required"*.

The **display is not** a difference: both boards carry the same **ST7789V2**,
1.14", 240 × 135, and M5GFX drives it identically.

## Memory — there is no PSRAM

The ADV's Stamp-S3A is an **ESP32-S3FN8** — dual-core Xtensa LX7 at 240 MHz,
**8 MB flash, no PSRAM**. The `FN8` suffix *is* the statement: 8 MB embedded
flash, no embedded PSRAM. The linker gives this build 327,680 bytes of DRAM.
That shapes the whole renderer:

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

Straight from the M5Stack documentation.

**GNSS (UART1)** — the two pins geoscout actually drives:

| Cardputer-Adv | G13 | G15 |
|---|---|---|
| Cap LoRa-1262 (GPS) | GPS-RX | GPS-TX |

Read the direction from the cap's side: **G13 is the Cardputer's TX** into the
receiver's RX, **G15 is the Cardputer's RX** from the receiver's TX. The ADV's
own header labels them `UART_TX = G13` and `UART_RX = G15`, and
`HardwareSerial::begin()` takes them in the order `(baud, config, rxPin, txPin)`
— so `begin(115200, SERIAL_8N1, 15, 13)`. Swapping the pair is silent: no error,
no bytes, an app that acquires forever.

**I²C** — one bus, five devices:

| Cardputer-Adv | G8 | G9 |
|---|---|---|
| TCA8418RTWR (keyboard) | SDA | SCL |
| ES8311 (codec) | SDA | SCL |
| BMI270 (IMU) | SDA | SCL |
| Cap LoRa-1262 (PI4IOE5V6408) | SDA | SCL |
| Grove HY2.0-4P Port.A | SDA | SCL |

**microSD** — geoscout opens none of this, but note which pins it is:

| Stamp-S3A | G12 | G14 | G40 | G39 |
|---|---|---|---|---|
| microSD | CS | MOSI | CLK | MISO |

**Display:**

| Stamp-S3A | G38 | G33 | G34 | G35 | G36 | G37 |
|---|---|---|---|---|---|---|
| ST7789V2 | DISP_BL | RST | RS | DAT | SCK | CS |

**EXT 2.54-14P header** (the ADV side, which is what the cap mates with):

| FUNC | PIN | LEFT | RIGHT | PIN | FUNC |
|---|---|---|---|---|---|
| RESET | G3 | 1 | 2 | 5VIN | |
| INT | G4 | 3 | 4 | GND | |
| BUSY | G6 | 5 | 6 | 5VOUT | |
| SCK | G40 | 7 | 8 | G8 | I2C_SDA |
| MOSI | G14 | 9 | 10 | G9 | I2C_SCL |
| MISO | G39 | 11 | 12 | G13 | UART_TX |
| CS | G5 | 13 | 14 | G15 | UART_RX |

Other things on the ADV worth knowing are there: IR TX on **G44**, battery ADC
on **G10**, Grove Port.CUSTOM on **G1/G2** (not I²C — that is Port.A on G8/G9).

## The radio geoscout does not use

Recorded for whoever adds it. **Semtech SX1262**, 868–923 MHz, +22 dBm max,
−147 dBm sensitivity, on SPI:

| Cardputer-Adv | G3 | G4 | G6 | G40 | G14 | G39 | G5 |
|---|---|---|---|---|---|---|---|
| Cap LoRa-1262 (LoRa) | RST | IRQ | BUSY | SCK | MOSI | MISO | NSS |

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

## Sources

- [M5Stack — Cardputer-ADV](https://docs.m5stack.com/en/core/Cardputer-ADV)
  (note the hyphen; `Cardputer%20ADV` 404s)
- [M5Stack — Cap LoRa-1262](https://docs.m5stack.com/en/cap/Cap_LoRa-1262), SKU U214
- [M5Stack — Cap LoRa868 Arduino tutorial](https://docs.m5stack.com/en/arduino/projects/cap/cap_lora868)
