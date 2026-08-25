#!/usr/bin/env python3
"""Turn Natural Earth GeoJSON into the flash-resident world geometry.

The renderer never sees a latitude. Every vertex is baked here into a unit
sphere vector, scaled to int16, so that drawing a frame is nine integer
multiply-adds per point and no trigonometry at all. At 240 MHz that is the
difference between a globe that spins and a globe that stutters.

Input layers (GeoJSON, from github.com/nvkelso/natural-earth-vector, which is
public domain):

    ne_110m_coastline                     LineString  / MultiLineString
    ne_110m_admin_0_boundary_lines_land   LineString  / MultiLineString
    ne_110m_lakes                         Polygon     / MultiPolygon

Usage:
    tools/genmap.py --in <dir-of-geojson> --out lib/core [--tol 0.25]
"""

import argparse
import json
import math
import os
import sys

# A polyline shorter than this after simplification is a speck at 240x135 and
# costs more in table overhead than it contributes in pixels.
MIN_POINTS = 2
MIN_SPAN_DEG = 0.6

LAYERS = [
    ("coast", "ne_110m_coastline.geojson"),
    ("border", "ne_110m_admin_0_boundary_lines_land.geojson"),
    ("lake", "ne_110m_lakes.geojson"),
]


def rings(geometry):
    """Yield coordinate lists from any GeoJSON geometry we care about."""
    kind = geometry["type"]
    coords = geometry["coordinates"]
    if kind == "LineString":
        yield coords
    elif kind == "MultiLineString":
        yield from coords
    elif kind == "Polygon":
        # Exterior ring only. At 110m an inner ring is a lake in an island in a
        # lake, and it is one pixel.
        yield coords[0]
    elif kind == "MultiPolygon":
        for poly in coords:
            yield poly[0]
    else:
        raise ValueError("unhandled geometry %s" % kind)


def perpendicular_distance(pt, start, end):
    (px, py), (sx, sy), (ex, ey) = pt, start, end
    dx, dy = ex - sx, ey - sy
    if dx == 0.0 and dy == 0.0:
        return math.hypot(px - sx, py - sy)
    t = ((px - sx) * dx + (py - sy) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    return math.hypot(px - (sx + t * dx), py - (sy + t * dy))


def douglas_peucker(points, tol):
    """Iterative Douglas-Peucker. Recursion blows the stack on Antarctica."""
    if len(points) < 3:
        return list(points)
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    while stack:
        first, last = stack.pop()
        if last <= first + 1:
            continue
        worst, worst_i = -1.0, first
        for i in range(first + 1, last):
            d = perpendicular_distance(points[i], points[first], points[last])
            if d > worst:
                worst, worst_i = d, i
        if worst > tol:
            keep[worst_i] = True
            stack.append((first, worst_i))
            stack.append((worst_i, last))
    return [p for p, k in zip(points, keep) if k]


def split_antimeridian(points):
    """Break a polyline wherever it wraps, so no segment crosses the seam.

    Natural Earth already splits most features at +/-180, but a few (Antarctica,
    the Russian far east) still carry a step. Left in place, one such vertex
    pair draws a chord straight through the middle of the globe.
    """
    out, run = [], [points[0]]
    for prev, cur in zip(points, points[1:]):
        if abs(cur[0] - prev[0]) > 180.0:
            out.append(run)
            run = [cur]
        else:
            run.append(cur)
    out.append(run)
    return [r for r in out if len(r) >= MIN_POINTS]


def span(points):
    lons = [p[0] for p in points]
    lats = [p[1] for p in points]
    return max(max(lons) - min(lons), max(lats) - min(lats))


def to_unit_i16(lon, lat):
    """Geographic degrees -> int16 unit sphere vector.

    +Z is the north pole, +X is (0N, 0E), +Y is (0N, 90E).
    """
    rlat, rlon = math.radians(lat), math.radians(lon)
    cl = math.cos(rlat)
    v = (cl * math.cos(rlon), cl * math.sin(rlon), math.sin(rlat))
    out = []
    for c in v:
        n = int(round(c * 32767.0))
        out.append(max(-32767, min(32767, n)))
    return tuple(out)


def load_layer(path, tol):
    with open(path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    polylines = []
    for feature in data["features"]:
        geometry = feature.get("geometry")
        if not geometry:
            continue
        for ring in rings(geometry):
            ring = [(float(c[0]), float(c[1])) for c in ring]
            if len(ring) < MIN_POINTS:
                continue
            for piece in split_antimeridian(ring):
                simplified = douglas_peucker(piece, tol)
                if len(simplified) < MIN_POINTS:
                    continue
                if span(simplified) < MIN_SPAN_DEG:
                    continue
                polylines.append(simplified)
    return polylines


def emit(out_dir, layers, tol):
    points, tables = [], {}
    for name, polylines in layers:
        table = []
        for line in polylines:
            first = len(points)
            for lon, lat in line:
                points.append(to_unit_i16(lon, lat))
            table.append((first, len(line)))
        tables[name] = table

    if len(points) > 65535:
        sys.exit("error: %d points overflows the uint16 vertex index" % len(points))

    header = os.path.join(out_dir, "worlddata.h")
    source = os.path.join(out_dir, "worlddata.cpp")

    with open(header, "w", encoding="utf-8") as fh:
        fh.write(
            "// Generated by tools/genmap.py -- do not edit.\n"
            "//\n"
            "// Natural Earth 1:110m, simplified at %g degrees, baked to int16\n"
            "// unit sphere vectors. Source data is public domain.\n"
            "#pragma once\n\n"
            "#include <cstdint>\n\n"
            "namespace geoscout {\n\n"
            "// One run of vertices in kWorldPoints.\n"
            "struct GeoPolyline {\n"
            "    uint16_t first;\n"
            "    uint16_t count;\n"
            "};\n\n"
            "// Unit sphere vectors scaled by 32767. +Z north, +X at 0N 0E.\n"
            "extern const int16_t kWorldPoints[][3];\n"
            "constexpr int kWorldPointCount = %d;\n\n" % (tol, len(points))
        )
        for name, _ in layers:
            fh.write(
                "extern const GeoPolyline k%sLines[];\n"
                "constexpr int k%sLineCount = %d;\n\n"
                % (name.capitalize(), name.capitalize(), len(tables[name]))
            )
        fh.write("}  // namespace geoscout\n")

    with open(source, "w", encoding="utf-8") as fh:
        fh.write(
            "// Generated by tools/genmap.py -- do not edit.\n"
            '#include "worlddata.h"\n\n'
            "namespace geoscout {\n\n"
            "const int16_t kWorldPoints[][3] = {\n"
        )
        for i in range(0, len(points), 4):
            chunk = points[i:i + 4]
            fh.write("    " + " ".join("{%d,%d,%d}," % p for p in chunk) + "\n")
        fh.write("};\n\n")
        for name, _ in layers:
            fh.write(
                "const GeoPolyline k%sLines[] = {\n" % name.capitalize()
            )
            table = tables[name]
            for i in range(0, len(table), 6):
                chunk = table[i:i + 6]
                fh.write("    " + " ".join("{%d,%d}," % e for e in chunk) + "\n")
            fh.write("};\n\n")
        fh.write("}  // namespace geoscout\n")

    return points, tables


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="src", required=True)
    ap.add_argument("--out", dest="dst", required=True)
    ap.add_argument("--tol", type=float, default=0.25,
                    help="Douglas-Peucker tolerance in degrees")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    layers = []
    for name, filename in LAYERS:
        path = os.path.join(args.src, filename)
        if not os.path.exists(path):
            sys.exit("error: missing %s" % path)
        layers.append((name, load_layer(path, args.tol)))

    total_points = sum(sum(len(l) for l in lines) for _, lines in layers)
    total_lines = sum(len(lines) for _, lines in layers)
    for name, lines in layers:
        print("  %-7s %5d polylines  %6d points"
              % (name, len(lines), sum(len(l) for l in lines)))
    print("  %-7s %5d polylines  %6d points  = %d bytes flash"
          % ("total", total_lines, total_points,
             total_points * 6 + total_lines * 4))

    if not args.dry_run:
        emit(args.dst, layers, args.tol)
        print("wrote %s/worlddata.{h,cpp}" % args.dst)


if __name__ == "__main__":
    main()
