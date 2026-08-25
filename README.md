# geoscout

[![CI](https://github.com/meister5/geoscout/actions/workflows/ci.yml/badge.svg)](https://github.com/meister5/geoscout/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**A pocket globe and GNSS console for the M5Stack Cardputer ADV with the Cap LoRa-1262.**

geoscout puts the whole planet on a 240×135 screen and a red dot on the part of
it you are standing on. It renders a shaded 3D Earth with real coastlines,
borders and lakes, spins it under your finger, draws the day/night terminator
where the sun actually puts it right now, and reads your position off the cap's
GNSS receiver in whatever format you need it in — decimal, DMS, UTM, MGRS or
Maidenhead.

There is no SD card involved and nothing is written to one. Everything here runs
out of flash and NVS.

> **Cardputer ADV only.** The Cap LoRa-1262 carries the ATGM336H GNSS receiver
> this firmware reads, and it mounts on the rear Cap-Bus header — present on the
> Cardputer ADV and CardputerZero, **absent on the original Cardputer v1.1**.
> The ADV's keyboard is also a different part (a TCA8418 I²C controller, not the
> v1.1's scanned matrix), and this firmware refuses to boot on anything else
> rather than drive the wrong pins. See [docs/HARDWARE.md](docs/HARDWARE.md).

---

## Apps

geoscout boots into a menu whose rows carry live subtitles — the globe row shows
where the view is pointed, the position row shows your current fix — so the menu
itself is a status screen.

| # | App | What it does |
|---|---|---|
| 1 | **Global Position** | Your coordinates, large, in five formats |
| 2 | **3D Globe** | A shaded Earth with the real terminator on it |
| 3 | **Sky View** | Which satellites are up, where, and how loud |
| 4 | **Settings** | Formats, units, time zone, brightness, globe layers |
| 5 | **About** | Version, hardware, attribution, free heap |

### Global Position

The fix, in whichever of these you asked for — `←`/`→` cycles without leaving
the screen:

```
Decimal      52.370216 N    4.895168 E
D M S        52 22' 12.8" N    4 53' 42.6" E
D M.m        52 22.213' N      4 53.710' E
Maidenhead   JO22ki
UTM          31U 629022E 5803906N
MGRS         31U FU 29022 03906
```

Below the readout: altitude, speed and course; satellite count, PDOP and the age
of the fix. Before the first fix it shows an acquiring screen with a per-
satellite C/N₀ bar, so a cold start looks like progress rather than like a hang.

MGRS and UTM are computed on WGS84 with k₀ = 0.9996, including the Norway zone-32
widening and the Svalbard 31/33/35/37 exceptions, and MGRS **truncates** rather
than rounds — a grid reference names the square you are standing in. Both are
checked against PROJ and against the NGA-derived `mgrs` library in the test
suite.

### 3D Globe

An orthographic Earth built from Natural Earth 1:110m vectors: 130 coastline
polylines, 308 borders and 22 lakes, 6,772 vertices in all, baked to flash as
int16 unit vectors and projected in fixed point. It costs 42 KB of flash and no
RAM at all.

The day/night terminator is not a decoration painted on top — it is the real one.
The subsolar point comes from the low-precision solar position algorithm in the
Astronomical Almanac, fed by GNSS time, and a point is lit exactly when its dot
product with the sun direction is positive. The ocean is filled by solving that
same sign test per pixel, which makes a properly curved terminator cost three
coefficients a frame and a handful of horizontal lines a row.

Three modes, cycled with `Enter`:

- **Follow** — the globe keeps your position under the centre
- **Free** — you fly it yourself
- **Spin** — it turns on its axis, which is what it does on a desk

| Key | |
|---|---|
| `;` `.` `,` `/` | rotate |
| `x` / `z` | zoom (radius 40–190 px) |
| `Enter` | cycle Follow → Free → Spin |
| `g` | graticule |
| `n` | night shading |
| `b` | borders |
| `l` | lakes |
| `h` | HUD |
| `` ` `` | back to the menu |

### Sky View

A polar plot of every satellite the receiver is tracking — azimuth around,
elevation in from the rim, rings at 30° and 60°. Satellites used in the fix are
filled; the rest are outlines. The right-hand panel is a C/N₀ bar chart sorted
loudest-first, each bar tagged with its RINEX constellation letter: **G**PS,
GLONASS (**R**), **E** Galileo, Bei**D**ou as **C**, QZSS as **J**.

The ATGM336H is multi-constellation, so on a clear sky this fills up.

## Building

```sh
pio run -e cardputer-adv -t upload      # flash over USB
make test                               # the host test suites
make preview                            # the host globe renderer
tools/package.sh                        # the release image, gated on size + magic
```

A prebuilt `geoscout-app.bin` sits at the repo root. Point M5Launcher's
**OTA → Favorites** at its raw URL, or copy it to the SD card and install it from
**SD**.

### Seeing the globe without a device

`lib/core` compiles with nothing but a C++17 compiler, and the geometry pass in
`lib/core/render.{h,cpp}` is shared verbatim between the firmware and a host
renderer. So you can look at exactly what the device would draw:

```sh
make preview
build/preview 52.37 4.89 1787579102 out.ppm     # lat lon unix-seconds
python3 tools/ppm2png.py out.ppm out.png 4      # 4x nearest-neighbour scale
```

That is how the terminator was verified: render Sydney at local midnight, and
the face is dark with a lit arc on the bottom limb — which is the South
Atlantic, where it genuinely is daytime.

### Regenerating the world

`lib/core/worlddata.cpp` is generated, and committed, so an ordinary build needs
no network and no Python:

```sh
python3 tools/genmap.py --tolerance 0.1
```

It reads the three Natural Earth GeoJSON layers, simplifies each ring with
Douglas-Peucker, splits the polylines that cross the antimeridian, and writes the
result as int16 triplets. Lower the tolerance for more coastline and more flash.

## Testing

Five suites, ~29,500 assertions, all on the host compiler with `-Werror`:

| Suite | What it pins down |
|---|---|
| `geo` | UTM/MGRS against PROJ and NGA, Maidenhead, distance, formatting |
| `nmea` | sentence parsing, checksums, GSV assembly, GSA used-in-fix runs |
| `solar` | subsolar point, sun altitude, daylight phase |
| `sphere` | projection stays inside the disc, basis stays orthonormal, limb clipping |
| `render` | ocean spans tile every row with no gap or overlap, terminator sides |

```sh
make test
```

## Layout

```
lib/core/       no Arduino headers, host-testable: geo, nmea, solar, sphere,
                render, and the generated worlddata
src/hal/        display, keys (TCA8418), gnss (UART1)
src/shell/      the launcher, settings, theme, and the App interface
src/apps/       position, globe, skyview, settings, about
tools/          genmap.py, preview.cpp, ppm2png.py, package.sh
```

Adding an app means subclassing `App`, implementing `name()` and `draw()`, and
registering it in `main.cpp`. The shell owns the display, the canvas, the
keyboard and the GNSS receiver, and hands each app a `Frame` with the current fix
and subsolar point already resolved.

## Credits

World geometry from [Natural Earth](https://www.naturalearthdata.com/) (public
domain). Solar position from the low-precision algorithm in the *Astronomical
Almanac*. Built on [M5Unified](https://github.com/m5stack/M5Unified) and
[M5GFX](https://github.com/m5stack/M5GFX).

MIT licensed — see [LICENSE](LICENSE), and [NOTICE](NOTICE) for the
third-party data and algorithm credits in full.
