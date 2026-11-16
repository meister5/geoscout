# geoscout

[![CI](https://github.com/meister5/geoscout/actions/workflows/ci.yml/badge.svg)](https://github.com/meister5/geoscout/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Pocket globe and GNSS console for the M5Stack Cardputer ADV with the Cap LoRa-1262.

## Features

- Shaded 3D Earth with coastlines, borders and lakes from Natural Earth 1:110m data
- Day/night terminator computed from GNSS time
- Position readout in decimal, DMS, DM.m, Maidenhead, UTM and MGRS
- Sky view with a satellite polar plot and C/N0 bars, tagged by constellation (G, R, E, C, J)
- Globe modes: Follow, Free, Spin
- Settings for coordinate format, units, time zone, brightness and globe layers
- No SD card needed; everything lives in flash and NVS

## Hardware

- M5Stack Cardputer ADV
- Cap LoRa-1262 (its ATGM336H GNSS receiver is what this firmware reads)

Cardputer ADV only. The cap needs the rear Cap-Bus header, which the original Cardputer v1.1 lacks, and the firmware halts on any other board. Details: [docs/HARDWARE.md](docs/HARDWARE.md).

## Install

Prebuilt image: [geoscout-app.bin](https://raw.githubusercontent.com/meister5/geoscout/main/geoscout-app.bin).

1. Copy `geoscout-app.bin` to the SD card and install it from `SD` in M5Launcher, or
2. point M5Launcher's `OTA > Favorites` at the raw URL above.

## Usage

Main menu: Global Position, 3D Globe, Sky View, Settings, About.

| Key | Action |
|---|---|
| `;` `.` | Menu up, down |
| `Enter`, `Space`, `/` | Open the selected app |
| `1`-`5` | Jump straight to an app |
| `` ` `` or `del` | Back |
| `,` `/` | Position screen: cycle coordinate format |

Globe:

| Key | Action |
|---|---|
| `;` `.` `,` `/` | Rotate |
| `x` / `z` | Zoom in / out |
| `Enter` | Cycle Follow, Free, Spin |
| `g` `n` `b` `l` `h` | Toggle graticule, night shading, borders, lakes, HUD |

## Build

Requires [PlatformIO](https://platformio.org/).

```sh
pio run -e cardputer-adv -t upload   # build and flash over USB
tools/package.sh                     # release image
make test                            # host tests for lib/core (C++17 compiler only)
make preview                         # host renderer for the globe
```

`lib/core/worlddata.cpp` is generated and committed. `python3 tools/genmap.py --tolerance 0.1` regenerates it from Natural Earth GeoJSON.

## Credits

World geometry from [Natural Earth](https://www.naturalearthdata.com/) (public domain). Solar position from the low-precision algorithm in the Astronomical Almanac. See [NOTICE](NOTICE).

## License

MIT. See [LICENSE](LICENSE).
