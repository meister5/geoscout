#include "render.h"

#include <cmath>

namespace geoscout {

void renderLayer(const GeoPolyline* lines, int lineCount, const FixedView& view,
                 int32_t radius, const int32_t* sunAxis, SegmentSink sink,
                 void* context) {
    if (lines == nullptr || sink == nullptr) return;

    for (int i = 0; i < lineCount; ++i) {
        const GeoPolyline& line = lines[i];
        Screen previous;
        bool previousLit = true;
        bool havePrevious = false;

        for (int j = 0; j < line.count; ++j) {
            const int16_t* point = kWorldPoints[line.first + j];
            const Screen current = project(point, view, radius);
            const bool currentLit = sunAxis == nullptr || dotFixed(point, sunAxis) > 0;

            if (havePrevious) {
                if (previous.front && current.front) {
                    sink(context, previous.x, previous.y, current.x, current.y, previousLit);
                } else if (previous.front != current.front) {
                    const Screen& visible = previous.front ? previous : current;
                    const Screen& hidden = previous.front ? current : previous;
                    int32_t edgeX = 0;
                    int32_t edgeY = 0;
                    if (clipToLimb(visible, hidden, radius, &edgeX, &edgeY)) {
                        sink(context, visible.x, visible.y, edgeX, edgeY,
                             previous.front ? previousLit : currentLit);
                    }
                }
            }

            previous = current;
            previousLit = currentLit;
            havePrevious = true;
        }
    }
}

void renderOcean(const View& view, const Vec3& sunDirection, int32_t radius,
                 int32_t clipTop, int32_t clipBottom, SpanSink sink, void* context) {
    if (sink == nullptr || radius <= 0) return;

    const float a = static_cast<float>(view.right.x * sunDirection.x +
                                       view.right.y * sunDirection.y +
                                       view.right.z * sunDirection.z);
    const float b = static_cast<float>(view.up.x * sunDirection.x +
                                       view.up.y * sunDirection.y +
                                       view.up.z * sunDirection.z);
    const float c = static_cast<float>(view.toward.x * sunDirection.x +
                                       view.toward.y * sunDirection.y +
                                       view.toward.z * sunDirection.z);

    const float invRadius = 1.0f / static_cast<float>(radius);
    int32_t top = clipTop < -radius ? -radius : clipTop;
    int32_t bottom = clipBottom > radius ? radius : clipBottom;

    for (int32_t dy = top; dy <= bottom; ++dy) {
        const int32_t halfWidth = static_cast<int32_t>(
            std::sqrt(static_cast<float>(radius * radius - dy * dy)));
        if (halfWidth <= 0) continue;

        const float v = -static_cast<float>(dy) * invRadius;
        const float bv = b * v;
        const float oneMinusV2 = 1.0f - v * v;

        const auto lit = [&](int32_t dx) {
            const float u = static_cast<float>(dx) * invRadius;
            const float w = oneMinusV2 - u * u;
            const float z = w > 0.0f ? std::sqrt(w) : 0.0f;
            return a * u + bv + c * z > 0.0f;
        };

        int32_t runStart = -halfWidth;
        bool runLit = lit(runStart);
        for (int32_t dx = -halfWidth + 1; dx <= halfWidth; ++dx) {
            const bool here = lit(dx);
            if (here == runLit) continue;
            sink(context, runStart, dy, dx - runStart, runLit);
            runStart = dx;
            runLit = here;
        }
        sink(context, runStart, dy, halfWidth + 1 - runStart, runLit);
    }
}

}  // namespace geoscout
