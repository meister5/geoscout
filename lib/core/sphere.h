// Orthographic projection of the unit sphere, in fixed point.
//
// The world geometry in worlddata.h is stored as int16 unit vectors, not as
// latitudes, so a frame costs three multiply-adds per point per axis and no
// trigonometry at all. Trigonometry happens once per frame, here, when the view
// basis is built.
//
// Host-testable: no Arduino, no display library.
#pragma once

#include <cstdint>

namespace geoscout {

// World vertices are Q15: a unit vector scaled by 32767.
constexpr int kPointShift = 15;
// The view basis is Q14, one bit coarser, so that the three-term dot product of
// a Q15 point with a Q14 basis vector cannot overflow int32. 3 * 2^29 is 1.6e9;
// int32 tops out at 2.1e9.
constexpr int kBasisShift = 14;
constexpr int32_t kBasisOne = 1 << kBasisShift;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// A geographic position as a unit vector. +Z is the north pole, +X is (0N, 0E).
Vec3 unitFromLatLon(double latDeg, double lonDeg);

// The inverse, for turning a point on screen back into a place.
void latLonFromUnit(const Vec3& v, double* outLatDeg, double* outLonDeg);

// An orthographic camera looking straight down at one point on the sphere,
// with north as close to screen-up as the geometry allows.
struct View {
    Vec3 right;    // screen +x
    Vec3 up;       // screen -y (screen y grows downward)
    Vec3 toward;   // out of the screen, toward the viewer

    // Centre the view on a geographic position. At the poles "north is up" has
    // no answer, so the meridian through lonDeg is used to break the tie and
    // the globe stays stable as you cross them.
    static View centeredOn(double latDeg, double lonDeg);
};

// The same basis, pre-scaled to Q14 for the per-vertex inner loop.
struct FixedView {
    int32_t right[3];
    int32_t up[3];
    int32_t toward[3];

    static FixedView from(const View& v);
};

// Where a world vertex lands, in whole pixels relative to the globe centre.
struct Screen {
    int32_t x = 0;       // right of centre
    int32_t y = 0;       // below centre
    int32_t depth = 0;   // Q14; positive means the near hemisphere
    bool front = false;
};

// `point` is an int16[3] Q15 unit vector; `radius` is the globe radius in
// pixels.
Screen project(const int16_t* point, const FixedView& view, int32_t radius);

// Clips a segment that crosses the limb, given one visible and one hidden
// endpoint. Writes the crossing point, pushed out onto the limb circle so the
// line ends exactly on the edge of the disc rather than a pixel inside it.
//
// `visible` and `hidden` must be in that order. Returns false if the two
// endpoints are degenerate.
bool clipToLimb(const Screen& visible, const Screen& hidden, int32_t radius,
                int32_t* outX, int32_t* outY);

// Q14 dot product of a world vertex with an arbitrary direction, which is how
// day/night is decided: positive means the vertex is on the lit side.
int32_t dotFixed(const int16_t* point, const int32_t* axis);

// Packs a double unit vector into the Q14 form `dotFixed` expects.
void toFixedAxis(const Vec3& v, int32_t* out);

}  // namespace geoscout
