#include "../support/check.h"
#include "render.h"
#include "solar.h"

#include <cmath>
#include <vector>

using namespace geoscout;

namespace {

constexpr int32_t kRadius = 62;

struct Segment {
    int32_t x0, y0, x1, y1;
    bool lit;
};

std::vector<Segment> g_segments;

void collectSegment(void* context, int32_t x0, int32_t y0, int32_t x1, int32_t y1, bool lit) {
    (void)context;
    g_segments.push_back(Segment{x0, y0, x1, y1, lit});
}

struct Span {
    int32_t x, y, width;
    bool lit;
};

std::vector<Span> g_spans;

void collectSpan(void* context, int32_t x, int32_t y, int32_t width, bool lit) {
    (void)context;
    g_spans.push_back(Span{x, y, width, lit});
}

double radius(int32_t x, int32_t y) {
    return std::sqrt(static_cast<double>(x) * x + static_cast<double>(y) * y);
}

}  // namespace

int main() {
    // A time and place with the terminator well inside the visible hemisphere,
    // so that both sides of it are exercised.
    const uint64_t when = 1787579102ull;    // 2026-08-24 13:45:02 UTC
    const SubsolarPoint sun = subsolarPoint(when);
    CHECK_TRUE(sun.valid);

    const View view = View::centeredOn(10.0, 64.0);
    const FixedView fixedView = FixedView::from(view);
    int32_t sunAxis[3];
    toFixedAxis(unitFromLatLon(sun.latDeg, sun.lonDeg), sunAxis);

    // --- layer rendering ----------------------------------------------------
    {
        g_segments.clear();
        renderLayer(kCoastLines, kCoastLineCount, fixedView, kRadius, sunAxis,
                    collectSegment, nullptr);

        // Half a world of coastline is a lot of segments and definitely not zero.
        CHECK_TRUE(g_segments.size() > 400);
        // ...and definitely not all of them: the far hemisphere is culled.
        CHECK_TRUE(g_segments.size() < static_cast<size_t>(kCoastLineCount) + 4057);

        bool anyLit = false;
        bool anyDark = false;
        for (const Segment& s : g_segments) {
            // Nothing may be drawn outside the disc. One pixel of slack for the
            // fixed-point rounding, as everywhere else.
            CHECK_TRUE(radius(s.x0, s.y0) <= kRadius + 1);
            CHECK_TRUE(radius(s.x1, s.y1) <= kRadius + 1);
            // A segment spanning more than the diameter would be a chord across
            // the face of the globe -- the exact failure the limb clipping is
            // there to prevent.
            CHECK_TRUE(radius(s.x1 - s.x0, s.y1 - s.y0) <= 2 * kRadius + 2);
            anyLit = anyLit || s.lit;
            anyDark = anyDark || !s.lit;
        }
        // This view was chosen to straddle the terminator; if it did not, the
        // day/night test below proves nothing.
        CHECK_TRUE(anyLit);
        CHECK_TRUE(anyDark);
    }

    // --- no terminator means everything is lit ------------------------------
    {
        g_segments.clear();
        renderLayer(kCoastLines, kCoastLineCount, fixedView, kRadius, nullptr,
                    collectSegment, nullptr);
        bool allLit = true;
        for (const Segment& s : g_segments) allLit = allLit && s.lit;
        CHECK_TRUE(allLit);
    }

    // --- a degenerate layer is not a crash ----------------------------------
    {
        g_segments.clear();
        renderLayer(nullptr, 10, fixedView, kRadius, sunAxis, collectSegment, nullptr);
        renderLayer(kCoastLines, 0, fixedView, kRadius, sunAxis, collectSegment, nullptr);
        renderLayer(kCoastLines, kCoastLineCount, fixedView, kRadius, sunAxis, nullptr, nullptr);
        CHECK_EQ(static_cast<long long>(g_segments.size()), 0ll);
    }

    // --- ocean spans tile the disc exactly ----------------------------------
    {
        g_spans.clear();
        renderOcean(view, unitFromLatLon(sun.latDeg, sun.lonDeg), kRadius, -kRadius,
                    kRadius, collectSpan, nullptr);
        CHECK_TRUE(!g_spans.empty());

        // Group by row and check that the spans cover it end to end with no gap
        // and no overlap. A gap shows up on the device as a scanline of
        // background straight through the ocean.
        for (int32_t row = -kRadius; row <= kRadius; ++row) {
            const int32_t half =
                static_cast<int32_t>(std::sqrt(static_cast<float>(kRadius * kRadius - row * row)));
            if (half <= 0) continue;

            int32_t cursor = -half;
            int32_t covered = 0;
            for (const Span& s : g_spans) {
                if (s.y != row) continue;
                CHECK_EQ(s.x, cursor);       // contiguous, in order
                CHECK_TRUE(s.width > 0);
                cursor += s.width;
                covered += s.width;
            }
            CHECK_EQ(covered, half * 2 + 1);
            CHECK_EQ(cursor, half + 1);
        }

        bool anyLit = false;
        bool anyDark = false;
        for (const Span& s : g_spans) {
            anyLit = anyLit || s.lit;
            anyDark = anyDark || !s.lit;
        }
        CHECK_TRUE(anyLit);
        CHECK_TRUE(anyDark);
    }

    // --- the sun overhead means a lit face ----------------------------------
    {
        // Looking straight down at the subsolar point the whole visible face is
        // in daylight, and the terminator coincides with the limb. Measured as
        // a fraction rather than as an absolute: exactly at the rim the dot
        // product is zero, which is neither side, and one pixel a frame lands
        // there. Anything worse than that is an inversion, not an edge case.
        const View noon = View::centeredOn(sun.latDeg, sun.lonDeg);
        g_spans.clear();
        renderOcean(noon, unitFromLatLon(sun.latDeg, sun.lonDeg), kRadius, -kRadius,
                    kRadius, collectSpan, nullptr);
        int32_t lit = 0;
        int32_t total = 0;
        for (const Span& s : g_spans) {
            total += s.width;
            if (s.lit) lit += s.width;
        }
        CHECK_TRUE(total > 10000);
        CHECK_TRUE(total - lit <= 4);

        // And from the antipode, none of it is.
        double antiLon = sun.lonDeg + 180.0;
        if (antiLon >= 180.0) antiLon -= 360.0;
        const View midnight = View::centeredOn(-sun.latDeg, antiLon);
        g_spans.clear();
        renderOcean(midnight, unitFromLatLon(sun.latDeg, sun.lonDeg), kRadius, -kRadius,
                    kRadius, collectSpan, nullptr);
        lit = 0;
        total = 0;
        for (const Span& s : g_spans) {
            total += s.width;
            if (s.lit) lit += s.width;
        }
        CHECK_TRUE(total > 10000);
        CHECK_TRUE(lit <= 4);
    }

    // --- clipping bounds the rows emitted -----------------------------------
    {
        // A globe zoomed past the edge of the screen must not cost rows that
        // are never seen.
        g_spans.clear();
        renderOcean(view, unitFromLatLon(sun.latDeg, sun.lonDeg), 190, -61, 61,
                    collectSpan, nullptr);
        CHECK_TRUE(!g_spans.empty());
        for (const Span& s : g_spans) {
            CHECK_TRUE(s.y >= -61);
            CHECK_TRUE(s.y <= 61);
        }
    }

    return check::finish("render");
}
