#include "../support/check.h"
#include "sphere.h"
#include "worlddata.h"

#include <cmath>

using namespace geoscout;

namespace {

// Pack a geographic position the way tools/genmap.py does, so the tests
// exercise exactly the encoding the flash tables use.
void bake(double latDeg, double lonDeg, int16_t* out) {
    const Vec3 v = unitFromLatLon(latDeg, lonDeg);
    out[0] = static_cast<int16_t>(std::lround(v.x * 32767.0));
    out[1] = static_cast<int16_t>(std::lround(v.y * 32767.0));
    out[2] = static_cast<int16_t>(std::lround(v.z * 32767.0));
}

}  // namespace

int main() {
    constexpr int32_t kRadius = 62;   // the globe at its default size

    // --- the unit vector encoding ------------------------------------------
    {
        const Vec3 origin = unitFromLatLon(0.0, 0.0);
        CHECK_NEAR(origin.x, 1.0, 1e-9);
        CHECK_NEAR(origin.y, 0.0, 1e-9);
        CHECK_NEAR(origin.z, 0.0, 1e-9);

        const Vec3 pole = unitFromLatLon(90.0, 0.0);
        CHECK_NEAR(pole.z, 1.0, 1e-9);

        const Vec3 east = unitFromLatLon(0.0, 90.0);
        CHECK_NEAR(east.y, 1.0, 1e-9);

        // Round trip through the inverse.
        double lat = 0.0, lon = 0.0;
        latLonFromUnit(unitFromLatLon(52.373056, 4.892222), &lat, &lon);
        CHECK_NEAR(lat, 52.373056, 1e-9);
        CHECK_NEAR(lon, 4.892222, 1e-9);
    }

    // --- the view basis is orthonormal --------------------------------------
    {
        const double samples[][2] = {{0, 0}, {52, 5}, {-33, 151}, {0, 179}, {45, -170}};
        for (const auto& s : samples) {
            const View v = View::centeredOn(s[0], s[1]);
            const auto len = [](const Vec3& a) {
                return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
            };
            const auto dot = [](const Vec3& a, const Vec3& b) {
                return a.x * b.x + a.y * b.y + a.z * b.z;
            };
            CHECK_NEAR(len(v.right), 1.0, 1e-9);
            CHECK_NEAR(len(v.up), 1.0, 1e-9);
            CHECK_NEAR(len(v.toward), 1.0, 1e-9);
            CHECK_NEAR(dot(v.right, v.up), 0.0, 1e-9);
            CHECK_NEAR(dot(v.right, v.toward), 0.0, 1e-9);
            CHECK_NEAR(dot(v.up, v.toward), 0.0, 1e-9);
        }
    }

    // --- projection ---------------------------------------------------------
    {
        const FixedView view = FixedView::from(View::centeredOn(0.0, 0.0));

        // The point under the camera lands dead centre, facing us.
        int16_t p[3];
        bake(0.0, 0.0, p);
        Screen s = project(p, view, kRadius);
        CHECK_TRUE(s.front);
        CHECK_EQ(s.x, 0);
        CHECK_EQ(s.y, 0);

        // North is up: screen y grows downward, so the pole is at negative y.
        // A pixel of slack throughout: the basis is quantised to Q14, so a
        // point on the limb can land one pixel inside it.
        bake(90.0, 0.0, p);
        s = project(p, view, kRadius);
        CHECK_EQ(s.x, 0);
        CHECK_TRUE(std::abs(s.y + kRadius) <= 1);
        CHECK_FALSE(s.front);       // exactly on the limb, not in front of it

        // East is to the right.
        bake(0.0, 90.0, p);
        s = project(p, view, kRadius);
        CHECK_TRUE(std::abs(s.x - kRadius) <= 1);
        CHECK_EQ(s.y, 0);

        // The far side of the world is culled.
        bake(0.0, 180.0, p);
        s = project(p, view, kRadius);
        CHECK_FALSE(s.front);
        // ...and so is anything more than a quarter turn away.
        bake(0.0, 100.0, p);
        CHECK_FALSE(project(p, view, kRadius).front);
        bake(0.0, 80.0, p);
        CHECK_TRUE(project(p, view, kRadius).front);
    }

    // --- everything projects inside the disc --------------------------------
    {
        const FixedView view = FixedView::from(View::centeredOn(37.0, -122.0));
        int16_t p[3];
        for (int lat = -90; lat <= 90; lat += 3) {
            for (int lon = -180; lon < 180; lon += 3) {
                bake(lat, lon, p);
                const Screen s = project(p, view, kRadius);
                const double r = std::sqrt(static_cast<double>(s.x) * s.x +
                                           static_cast<double>(s.y) * s.y);
                // One pixel of slack for the fixed-point rounding.
                CHECK_TRUE(r <= kRadius + 1);
            }
        }
    }

    // --- following your own position ----------------------------------------
    {
        // Centring on a place must put that place at the centre, whatever the
        // latitude -- including directly over a pole, where "north is up" has
        // no answer and the basis has to degenerate gracefully.
        const double places[][2] = {{52.37, 4.89}, {-33.86, 151.21}, {90.0, 0.0},
                                    {-90.0, 0.0}, {0.0, -180.0}};
        for (const auto& place : places) {
            const FixedView view = FixedView::from(View::centeredOn(place[0], place[1]));
            int16_t p[3];
            bake(place[0], place[1], p);
            const Screen s = project(p, view, kRadius);
            CHECK_TRUE(s.front);
            CHECK_TRUE(std::abs(s.x) <= 1);
            CHECK_TRUE(std::abs(s.y) <= 1);
        }
    }

    // --- limb clipping ------------------------------------------------------
    {
        const FixedView view = FixedView::from(View::centeredOn(0.0, 0.0));
        int16_t a[3], b[3];
        bake(0.0, 80.0, a);     // in front
        bake(0.0, 100.0, b);    // behind
        const Screen sa = project(a, view, kRadius);
        const Screen sb = project(b, view, kRadius);
        CHECK_TRUE(sa.front);
        CHECK_FALSE(sb.front);

        int32_t x = 0, y = 0;
        CHECK_TRUE(clipToLimb(sa, sb, kRadius, &x, &y));
        // The crossing must land on the limb circle, not inside it.
        const double r = std::sqrt(static_cast<double>(x) * x + static_cast<double>(y) * y);
        CHECK_NEAR(r, kRadius, 1.0);
        // ...and on the correct side of the globe.
        CHECK_TRUE(x > 0);

        // Two points at the same depth have no crossing to report.
        CHECK_FALSE(clipToLimb(sa, sa, kRadius, &x, &y));
    }

    // --- day/night is a sign test -------------------------------------------
    {
        int32_t sun[3];
        toFixedAxis(unitFromLatLon(0.0, 0.0), sun);   // sun over the Gulf of Guinea

        int16_t p[3];
        bake(0.0, 0.0, p);
        CHECK_TRUE(dotFixed(p, sun) > 0);             // local noon
        bake(0.0, 180.0, p);
        CHECK_TRUE(dotFixed(p, sun) < 0);             // local midnight
        bake(0.0, 90.0, p);
        CHECK_TRUE(std::abs(dotFixed(p, sun)) < 64);  // on the terminator
    }

    // --- the generated world data ------------------------------------------
    {
        struct Layer { const GeoPolyline* lines; int count; };
        const Layer layers[] = {
            {kCoastLines, kCoastLineCount},
            {kBorderLines, kBorderLineCount},
            {kLakeLines, kLakeLineCount},
        };

        int totalPoints = 0;
        for (const Layer& layer : layers) {
            CHECK_TRUE(layer.count > 0);
            for (int i = 0; i < layer.count; ++i) {
                const GeoPolyline& line = layer.lines[i];
                // A one-point polyline draws nothing and indicates a generator
                // bug, not a small island.
                CHECK_TRUE(line.count >= 2);
                CHECK_TRUE(static_cast<int>(line.first) + line.count <= kWorldPointCount);
                totalPoints += line.count;
            }
        }
        CHECK_TRUE(totalPoints <= kWorldPointCount);

        // Every baked vertex must actually be on the unit sphere, which is the
        // one invariant the whole renderer rests on.
        for (int i = 0; i < kWorldPointCount; ++i) {
            const double x = kWorldPoints[i][0] / 32767.0;
            const double y = kWorldPoints[i][1] / 32767.0;
            const double z = kWorldPoints[i][2] / 32767.0;
            CHECK_TRUE(std::fabs(std::sqrt(x * x + y * y + z * z) - 1.0) < 1e-3);
        }

        // No polyline may span more than a hemisphere between consecutive
        // points: that is the signature of an unsplit antimeridian crossing,
        // which draws a chord straight through the globe.
        for (const Layer& layer : layers) {
            for (int i = 0; i < layer.count; ++i) {
                const GeoPolyline& line = layer.lines[i];
                for (int j = line.first + 1; j < line.first + line.count; ++j) {
                    double ax, ay, az, bx, by, bz;
                    ax = kWorldPoints[j - 1][0] / 32767.0;
                    ay = kWorldPoints[j - 1][1] / 32767.0;
                    az = kWorldPoints[j - 1][2] / 32767.0;
                    bx = kWorldPoints[j][0] / 32767.0;
                    by = kWorldPoints[j][1] / 32767.0;
                    bz = kWorldPoints[j][2] / 32767.0;
                    CHECK_TRUE(ax * bx + ay * by + az * bz > 0.0);
                }
            }
        }
    }

    return check::finish("sphere");
}
