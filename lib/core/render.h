// The globe's geometry pass, with no display library in it.
//
// Everything that decides *where* something is drawn lives here; the app is
// left with the part that decides what colour it is and which library call
// puts it there. That split is what lets tools/preview.cpp render the same
// globe on a laptop as the firmware renders on the device, and what lets the
// native tests assert on segments instead of on pixels.
#pragma once

#include <cstdint>

#include "sphere.h"
#include "worlddata.h"

namespace geoscout {

// Called once per visible line segment, in screen pixels relative to the centre
// of the globe. `lit` is the day/night side the segment belongs to.
using SegmentSink = void (*)(void* context, int32_t x0, int32_t y0, int32_t x1,
                             int32_t y1, bool lit);

// Called once per horizontal run of ocean, in the same relative coordinates.
using SpanSink = void (*)(void* context, int32_t x, int32_t y, int32_t width, bool lit);

// Projects one layer of the world and emits its visible segments.
//
// A segment with both ends on the near hemisphere is emitted whole. One that
// crosses the silhouette is cut at the limb -- without that, a coastline
// disappearing round the back draws a chord across the face of the globe.
// A segment with both ends hidden is not emitted at all, which is what makes
// this cost roughly half the vertex count in draw calls.
//
// `sunAxis` may be null, meaning "no terminator": every segment comes back lit.
void renderLayer(const GeoPolyline* lines, int lineCount, const FixedView& view,
                 int32_t radius, const int32_t* sunAxis, SegmentSink sink,
                 void* context);

// Emits the ocean disc as horizontal runs, split where the terminator crosses.
//
// For a screen pixel at (u, v) in units of the globe radius, the point on the
// near hemisphere is u*right + v*up + z*toward with z = sqrt(1 - u^2 - v^2), so
// its dot product with the sun is a*u + b*v + c*z for three coefficients that
// only have to be found once per frame. That makes a filled terminator a sign
// test per pixel and a handful of horizontal lines per row.
//
// `clipTop` and `clipBottom` bound the rows emitted, relative to the centre, so
// a globe zoomed past the edge of the screen does not cost rows nobody sees.
void renderOcean(const View& view, const Vec3& sunDirection, int32_t radius,
                 int32_t clipTop, int32_t clipBottom, SpanSink sink, void* context);

}  // namespace geoscout
