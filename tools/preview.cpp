// Renders the globe on the host, through the same lib/core code the firmware
// uses, and writes it out as a PPM.
//
// This exists because the alternative way to find out whether the world looks
// like the world is to flash a device and squint at a 240x135 panel. It shares
// renderLayer() and renderOcean() with the app, so a geometry bug shows up here
// exactly as it would there -- only the colour lookup and the line drawing are
// reimplemented.
//
// Build and run:
//   make preview
//   build/preview 52.37 4.89 1787579102 out.ppm

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "geo.h"
#include "render.h"
#include "solar.h"
#include "sphere.h"
#include "worlddata.h"

using namespace geoscout;

namespace {

constexpr int kWidth = 240;
constexpr int kHeight = 135;
constexpr int kStatusBarHeight = 12;
constexpr int kCentreX = kWidth / 2;
constexpr int kCentreY = kStatusBarHeight + (kHeight - kStatusBarHeight) / 2;

struct Rgb {
    uint8_t r, g, b;
};

// The same palette as src/shell/theme.h, in the RGB888 those RGB565 literals
// were derived from.
constexpr Rgb kBackground{6, 8, 14};
constexpr Rgb kOceanDay{8, 76, 92};
constexpr Rgb kOceanNight{0, 36, 36};
constexpr Rgb kCoastDay{168, 238, 255};
constexpr Rgb kCoastNight{40, 138, 138};
constexpr Rgb kBorderDay{88, 102, 184};
constexpr Rgb kBorderNight{24, 24, 60};
constexpr Rgb kLakeDay{72, 172, 255};
constexpr Rgb kLakeNight{16, 20, 84};
constexpr Rgb kGraticule{32, 36, 36};
constexpr Rgb kSun{255, 228, 0};
constexpr Rgb kHere{248, 60, 60};
constexpr Rgb kPanel{24, 28, 38};

std::vector<Rgb> g_pixels(kWidth * kHeight, kBackground);

void plot(int x, int y, Rgb colour) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
    g_pixels[static_cast<size_t>(y) * kWidth + x] = colour;
}

void hline(int x, int y, int width, Rgb colour) {
    for (int i = 0; i < width; ++i) plot(x + i, y, colour);
}

void line(int x0, int y0, int x1, int y1, Rgb colour) {
    // Bresenham, matching what the display library does closely enough for the
    // purpose: this is a check on geometry, not on rasterisation.
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        plot(x0, y0, colour);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void disc(int cx, int cy, int radius, Rgb colour) {
    for (int dy = -radius; dy <= radius; ++dy) {
        const int half = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - dy * dy)));
        hline(cx - half, cy + dy, half * 2 + 1, colour);
    }
}

void ring(int cx, int cy, int radius, Rgb colour) {
    for (int a = 0; a < 360; ++a) {
        const double r = a * 3.14159265358979 / 180.0;
        plot(cx + static_cast<int>(std::lround(radius * std::cos(r))),
             cy + static_cast<int>(std::lround(radius * std::sin(r))), colour);
    }
}

struct LayerContext {
    Rgb day, night;
};

void emitSegment(void* context, int32_t x0, int32_t y0, int32_t x1, int32_t y1, bool lit) {
    const LayerContext* ctx = static_cast<const LayerContext*>(context);
    line(kCentreX + x0, kCentreY + y0, kCentreX + x1, kCentreY + y1,
         lit ? ctx->day : ctx->night);
}

void emitSpan(void* context, int32_t x, int32_t y, int32_t width, bool lit) {
    const LayerContext* ctx = static_cast<const LayerContext*>(context);
    hline(kCentreX + x, kCentreY + y, width, lit ? ctx->day : ctx->night);
}

void bake(double lat, double lon, int16_t* out) {
    const Vec3 v = unitFromLatLon(lat, lon);
    out[0] = static_cast<int16_t>(std::lround(v.x * 32767.0));
    out[1] = static_cast<int16_t>(std::lround(v.y * 32767.0));
    out[2] = static_cast<int16_t>(std::lround(v.z * 32767.0));
}

void drawGraticule(const FixedView& view, int radius) {
    int16_t point[3];
    const auto stroke = [&](bool meridian, double fixedDeg) {
        Screen previous;
        bool have = false;
        for (int step = -90; step <= 90; step += 10) {
            const int span = meridian ? step : step * 2;
            if (meridian) bake(static_cast<double>(span), fixedDeg, point);
            else bake(fixedDeg, static_cast<double>(span), point);
            const Screen current = project(point, view, radius);
            if (have && previous.front && current.front) {
                line(kCentreX + previous.x, kCentreY + previous.y,
                     kCentreX + current.x, kCentreY + current.y, kGraticule);
            }
            previous = current;
            have = true;
        }
    };
    for (int lon = -180; lon < 180; lon += 30) stroke(true, static_cast<double>(lon));
    for (int lat = -60; lat <= 60; lat += 30) stroke(false, static_cast<double>(lat));
    stroke(false, 0.0);
}

bool writePpm(const char* path) {
    FILE* fh = std::fopen(path, "wb");
    if (fh == nullptr) return false;
    std::fprintf(fh, "P6\n%d %d\n255\n", kWidth, kHeight);
    std::fwrite(g_pixels.data(), sizeof(Rgb), g_pixels.size(), fh);
    std::fclose(fh);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr,
                     "usage: preview <lat> <lon> <unix-seconds> <out.ppm> [radius]\n");
        return 2;
    }
    const double lat = std::atof(argv[1]);
    const double lon = std::atof(argv[2]);
    const uint64_t when = static_cast<uint64_t>(std::atoll(argv[3]));
    const char* out = argv[4];
    const int radius = argc > 5 ? std::atoi(argv[5]) : 62;

    const View view = View::centeredOn(lat, lon);
    const FixedView fixedView = FixedView::from(view);
    const SubsolarPoint sun = subsolarPoint(when);

    // Status bar, so the framing matches the device.
    for (int y = 0; y < kStatusBarHeight; ++y) hline(0, y, kWidth, kPanel);

    if (sun.valid) {
        LayerContext ocean{kOceanDay, kOceanNight};
        renderOcean(view, unitFromLatLon(sun.latDeg, sun.lonDeg), radius, -kCentreY,
                    kHeight - 1 - kCentreY, emitSpan, &ocean);
    } else {
        disc(kCentreX, kCentreY, radius, kOceanDay);
    }

    drawGraticule(fixedView, radius);

    int32_t sunAxis[3] = {0, 0, 0};
    const bool shade = sun.valid;
    if (shade) toFixedAxis(unitFromLatLon(sun.latDeg, sun.lonDeg), sunAxis);
    const int32_t* axis = shade ? sunAxis : nullptr;

    LayerContext borders{kBorderDay, kBorderNight};
    renderLayer(kBorderLines, kBorderLineCount, fixedView, radius, axis, emitSegment, &borders);
    LayerContext lakes{kLakeDay, kLakeNight};
    renderLayer(kLakeLines, kLakeLineCount, fixedView, radius, axis, emitSegment, &lakes);
    LayerContext coast{kCoastDay, kCoastNight};
    renderLayer(kCoastLines, kCoastLineCount, fixedView, radius, axis, emitSegment, &coast);

    int16_t point[3];
    if (sun.valid) {
        bake(sun.latDeg, sun.lonDeg, point);
        const Screen s = project(point, fixedView, radius);
        if (s.front) disc(kCentreX + s.x, kCentreY + s.y, 2, kSun);
    }

    bake(lat, lon, point);
    const Screen here = project(point, fixedView, radius);
    if (here.front) {
        ring(kCentreX + here.x, kCentreY + here.y, 6, kHere);
        disc(kCentreX + here.x, kCentreY + here.y, 2, kHere);
    }

    if (!writePpm(out)) {
        std::fprintf(stderr, "preview: cannot write %s\n", out);
        return 1;
    }
    std::printf("%s  centre %.2f,%.2f  sun %.2f,%.2f  radius %d\n", out, lat, lon,
                sun.latDeg, sun.lonDeg, radius);
    return 0;
}
