# Design

Two decisions shape this firmware. The first is that a launcher with an app
interface costs almost nothing and makes every later app cheap. The second is
that a spinning shaded Earth is affordable on a 240 MHz microcontroller with no
FPU worth using and no PSRAM — *if* you never compute a trigonometric function
at runtime.

## The shell

`main.cpp` owns nothing but startup order. Everything after it belongs to
`Shell`, which owns the display, the canvas, the keyboard and the GNSS receiver,
and hands each app a `Frame`:

```cpp
struct Frame {
    uint64_t nowMs;
    uint32_t dtMs;
    const GnssFix* fix;
    uint32_t fixAgeMs;
    bool everHadFix;
    uint64_t unixSeconds;
    SubsolarPoint sun;
    bool hasFix() const;
};
```

An app is a subclass of `App` with a `name()` and a `draw()`. Everything else —
`onKey`, `update`, `onEnter`, `onExit`, `menuLine`, `desiredFps` — has a default.
Registering it in `main.cpp` puts it in the menu.

Three things follow from the shell owning GNSS rather than each app opening it:

- **The subsolar point is resolved once per frame**, not once per app that wants
  it. The globe and the status bar see the same sun.
- **Apps do not lose the fix when you leave them.** Switching from Position to
  Globe does not restart acquisition, because nothing was ever stopped.
- **Input and GNSS are serviced every pass; drawing is not.** `tick()` polls the
  keyboard and drains the UART unconditionally, then draws only if
  `app->desiredFps()` says it is time. A settings screen at 5 fps still reads
  keys as fast as the globe at 20.

`menuLine()` is why the launcher is worth having: each row carries a live
subtitle — where the globe is pointed, what the current fix is, how many
satellites are up — so the menu is itself a status screen rather than a list of
words.

### Settings live in NVS, one key per field

No blob, no struct dump. A blob makes every field change a migration; separate
keys mean a new setting defaults cleanly on firmware that has never seen it, and
an old one goes away by simply not being read. There is no SD card in wave 1 at
all, and nothing is written to one.

## The globe

### Why the geometry is baked

The world is 6,772 vertices. Turning a (lat, lon) pair into a unit vector costs
two sines and two cosines; at 30 fps that would be **over 800,000 trig calls per
second** before any projection happened. The S3 cannot hold that and still draw.

So `tools/genmap.py` does it once, at build time, and emits each vertex as an
int16 XYZ triplet scaled by 32767:

```python
def to_unit_i16(lon, lat):
    rlat, rlon = math.radians(lat), math.radians(lon)
    cl = math.cos(rlat)
    v = (cl * math.cos(rlon), cl * math.sin(rlon), math.sin(rlat))
```

What ships is `lib/core/worlddata.cpp` — `const` data in flash, 42,472 bytes,
**zero RAM**. Runtime cost per vertex drops to nine integer multiplies.

The generator also does the two jobs that are invisible until they go wrong:

- **Douglas-Peucker simplification**, *iteratively*. The recursive formulation
  overflows the stack on Antarctica, which is one polygon with thousands of
  points. At 0.1° tolerance the coastline goes from 4,057 points to 130
  polylines' worth, and the world still reads as the world.
- **Antimeridian splitting.** A polyline that steps from +179° to −179° is a
  short hop in reality and a line straight across the globe on screen.

Lower `--tolerance` for more coastline and more flash; nothing else changes.

### Fixed-point projection, and the two shifts that matter

`View::centeredOn(lat, lon)` builds an orthonormal basis: `toward` is the unit
vector at that point, `up` is north made perpendicular to it (with a meridian
fallback at the poles, where north is undefined), `right` is `up × toward`.

Projection is three dot products. The scales are chosen so they cannot overflow:

- world vertices are **Q15** (×32767)
- the view basis is **Q14** (×16384)

so a three-term int32 dot product peaks at 3 × 2²⁹ ≈ 1.6 × 10⁹, inside int32's
2.1 × 10⁹. Q15 on both sides would not fit.

Two subtleties cost real pixels:

**Round, don't shift.** An arithmetic right shift floors, which for negative
numbers grows the magnitude. Shifting rather than rounding makes the left and
top of the globe sit one pixel further out than the right and bottom — a
lopsided Earth, from nothing but `>>`:

```cpp
constexpr int32_t kHalf = 1 << (kBasisShift - 1);
s.x = (dx * radius + kHalf) >> kBasisShift;
```

**Clip to the limb.** A segment with one end on the far hemisphere must be cut
where it crosses the silhouette. `clipToLimb()` interpolates to depth zero and
then rescales the result onto the limb circle. Skip it and every coastline
disappearing round the back draws a chord straight across the face of the globe.
`test/test_render` asserts exactly that: no emitted segment is longer than the
diameter.

### The terminator is a sign test

A point on the sphere is in daylight when its dot product with the unit vector
toward the sun is positive. That is the whole of it — no shadow volumes, no
per-pixel lighting model.

For the outlines that is one dot product per vertex. For the **filled ocean** it
is better than that. A screen pixel at (u, v), in units of the globe radius,
corresponds to the surface point

```
u·right + v·up + z·toward,     z = sqrt(1 − u² − v²)
```

so its dot product with the sun is `a·u + b·v + c·z`, where `a`, `b` and `c`
depend only on the view and the sun — **three coefficients per frame**. Each row
then walks left to right, flips a boolean where the sign changes, and emits
horizontal runs. A correctly curved terminator, filled, costs a sqrt per pixel
and a handful of `drawFastHLine` calls per row.

The subsolar point itself comes from the low-precision solar position algorithm
in the *Astronomical Almanac*, fed by GNSS time. Before the first fix there is no
time, `SubsolarPoint::valid` is false, and the globe draws unshaded rather than
lit by a sun at the epoch.

### Sharing the geometry pass with the host

`lib/core/render.{h,cpp}` contains everything that decides *where* something is
drawn, and nothing that knows about a display library. It emits through two
function pointers:

```cpp
using SegmentSink = void (*)(void*, int32_t x0, int32_t y0, int32_t x1, int32_t y1, bool lit);
using SpanSink    = void (*)(void*, int32_t x, int32_t y, int32_t width, bool lit);
```

The firmware's sinks call `M5Canvas`. `tools/preview.cpp`'s sinks write pixels
into a PPM. They run **the same geometry**, so a projection bug shows up on a
laptop exactly as it would on the device — which is how the terminator was
checked without flashing anything. Render Sydney at local midnight and the face
is dark with a lit arc at the bottom limb; that arc is the South Atlantic, where
it is genuinely daytime.

It also makes the renderer testable at all. `test/test_render` asserts on
segments and spans — that the ocean's spans tile every row exactly, contiguous,
in order, with no gap and no overlap — which is not something you can assert
about a framebuffer.

## What is deliberately not here

- **No SD card.** Nothing is written to one and none is required. When an app
  eventually needs storage it gets a single `/geoscout/` directory, created on
  demand, never at boot.
- **No radio.** The SX1262 is not initialised. See
  [HARDWARE.md](HARDWARE.md) for what it would take.
- **No floating-point in the hot path.** The projection is integer. The ocean
  fill uses float for the per-pixel `sqrt` because that one is genuinely worth
  it, and it is the only place.
- **No per-frame allocation.** One canvas, allocated once at boot, reused
  forever.

## Testing

`lib/core` includes no Arduino headers, so all of it compiles and runs on the
host with `-Werror`. That covers the parts where being wrong is quiet: NMEA
parsing, coordinate conversion, solar position, the projection and the renderer.

Two of those are checked against outside authorities rather than against
themselves — UTM against **PROJ** (`pyproj`, EPSG:4326 → EPSG:32618) and MGRS
against the NGA-derived **`mgrs`** library. That is worth doing, because during
development the UTM suite failed against a hand-written expectation and the
implementation turned out to be right to the millimetre: the *reference* was
wrong. An assertion you wrote from memory only tests your memory.

The one place the tests are deliberately loose is the exact silhouette. At the
rim the dot product with the sun is exactly zero, which is neither day nor
night, and one pixel a frame lands there. `test_render` asserts the lit fraction
rather than an absolute — anything worse than a few pixels is an inversion, not
an edge case.
