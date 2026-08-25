#include "sphere.h"

#include <cmath>

namespace geoscout {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3{a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
}

double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 normalize(const Vec3& v) {
    const double len = std::sqrt(dot(v, v));
    if (len < 1e-12) return Vec3{0.0, 0.0, 1.0};
    return Vec3{v.x / len, v.y / len, v.z / len};
}

int32_t toQ14(double c) {
    long v = std::lround(c * static_cast<double>(kBasisOne));
    if (v > kBasisOne) v = kBasisOne;
    if (v < -kBasisOne) v = -kBasisOne;
    return static_cast<int32_t>(v);
}

void fill(int32_t* out, const Vec3& v) {
    out[0] = toQ14(v.x);
    out[1] = toQ14(v.y);
    out[2] = toQ14(v.z);
}

}  // namespace

Vec3 unitFromLatLon(double latDeg, double lonDeg) {
    const double lat = latDeg * kDegToRad;
    const double lon = lonDeg * kDegToRad;
    const double c = std::cos(lat);
    return Vec3{c * std::cos(lon), c * std::sin(lon), std::sin(lat)};
}

void latLonFromUnit(const Vec3& v, double* outLatDeg, double* outLonDeg) {
    const Vec3 n = normalize(v);
    if (outLatDeg != nullptr) {
        double z = n.z;
        if (z > 1.0) z = 1.0;
        if (z < -1.0) z = -1.0;
        *outLatDeg = std::asin(z) * kRadToDeg;
    }
    if (outLonDeg != nullptr) {
        *outLonDeg = std::atan2(n.y, n.x) * kRadToDeg;
    }
}

View View::centeredOn(double latDeg, double lonDeg) {
    View v;
    v.toward = unitFromLatLon(latDeg, lonDeg);

    // Screen up is the part of the north pole direction that is perpendicular
    // to the view axis. Directly over a pole that vector vanishes, so fall back
    // to the far side of the centre meridian, which is the direction "north"
    // degenerates into as you arrive.
    const Vec3 northPole{0.0, 0.0, 1.0};
    const double along = dot(northPole, v.toward);
    Vec3 up{northPole.x - along * v.toward.x,
            northPole.y - along * v.toward.y,
            northPole.z - along * v.toward.z};

    if (std::sqrt(dot(up, up)) < 1e-6) {
        const Vec3 meridian = unitFromLatLon(0.0, lonDeg + 180.0);
        const double a = dot(meridian, v.toward);
        up = Vec3{meridian.x - a * v.toward.x,
                  meridian.y - a * v.toward.y,
                  meridian.z - a * v.toward.z};
    }

    v.up = normalize(up);
    // right = up x toward gives a right-handed basis with `toward` out of the
    // screen, so that longitude increases to the right as you would expect.
    v.right = normalize(cross(v.up, v.toward));
    return v;
}

FixedView FixedView::from(const View& v) {
    FixedView f;
    fill(f.right, v.right);
    fill(f.up, v.up);
    fill(f.toward, v.toward);
    return f;
}

int32_t dotFixed(const int16_t* point, const int32_t* axis) {
    // Q15 * Q14 = Q29, summed three ways. Shifted back to Q14 so the caller can
    // treat the result as a signed fraction of one.
    const int32_t acc = static_cast<int32_t>(point[0]) * axis[0] +
                        static_cast<int32_t>(point[1]) * axis[1] +
                        static_cast<int32_t>(point[2]) * axis[2];
    return acc >> kPointShift;
}

void toFixedAxis(const Vec3& v, int32_t* out) {
    fill(out, normalize(v));
}

Screen project(const int16_t* point, const FixedView& view, int32_t radius) {
    Screen s;
    s.depth = dotFixed(point, view.toward);
    s.front = s.depth > 0;
    const int32_t dx = dotFixed(point, view.right);
    const int32_t dy = dotFixed(point, view.up);
    // Round rather than shift: an arithmetic shift floors, which grows the
    // magnitude of negative coordinates and would make the left and top of the
    // globe sit a pixel further out than the right and bottom.
    constexpr int32_t kHalf = 1 << (kBasisShift - 1);
    s.x = (dx * radius + kHalf) >> kBasisShift;
    s.y = -((dy * radius + kHalf) >> kBasisShift);   // screen y grows downward
    return s;
}

bool clipToLimb(const Screen& visible, const Screen& hidden, int32_t radius,
                int32_t* outX, int32_t* outY) {
    const int32_t span = visible.depth - hidden.depth;
    if (span == 0) return false;

    // Where along the segment the depth reaches zero -- that is the silhouette.
    const double t = static_cast<double>(visible.depth) / static_cast<double>(span);
    double x = visible.x + t * (hidden.x - visible.x);
    double y = visible.y + t * (hidden.y - visible.y);

    // Interpolating in screen space lands slightly inside the disc, because a
    // chord is shorter than its arc. Push it back out so outlines meet the edge.
    const double len = std::sqrt(x * x + y * y);
    if (len > 1e-6) {
        const double scale = static_cast<double>(radius) / len;
        x *= scale;
        y *= scale;
    }

    if (outX != nullptr) *outX = static_cast<int32_t>(std::lround(x));
    if (outY != nullptr) *outY = static_cast<int32_t>(std::lround(y));
    return true;
}

}  // namespace geoscout
