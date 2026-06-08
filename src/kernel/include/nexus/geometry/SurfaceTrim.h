#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — surface trim curves
//
//  A trim curve is a 2-D parametric curve in (u, v) surface parameter space
//  that defines the boundary of a trimmed region.  Trim loops partition a
//  NURBS surface into "keep" and "discard" regions.
//
//  Representation:
//    - Each trim curve is a 2-D piecewise-linear or NurbsCurve-like path
//      in (u, v) space.  For simplicity this implementation stores the
//      polyline as a sequence of (u, v) parameter samples; the caller is
//      responsible for sampling a NurbsCurve in parameter space first.
//    - A TrimLoop is a closed ordered sequence of TrimCurveSegment objects.
//    - A TrimRegion contains an outer loop (boundary) and zero or more inner
//      loops (holes).
//
//  Tessellation:
//    - `SurfaceTrimmer::tessellate()` samples the surface on a regular grid
//      and discards samples that fall outside all trim regions.  The surviving
//      samples are reconnected into a Mesh.
//
//  Point-in-loop test:
//    - 2-D winding number / ray-casting for (u, v) vs a trim loop polyline.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct TrimPoint {
    double u = 0.0;
    double v = 0.0;
};

// An ordered polyline in (u,v) space.  Close by repeating the first point
// as the last to form a closed loop, or leave open for an open trim edge.
struct TrimCurveSegment {
    std::vector<TrimPoint> points; // ordered parameter-space samples
    bool closed = true;            // true if the segment forms a loop by itself
};

// A closed trim loop composed of one or more connected curve segments.
struct TrimLoop {
    std::vector<TrimCurveSegment> segments;

    // True if the point (u, v) is inside this loop (2-D ray-cast).
    [[nodiscard]] bool contains(double u, double v) const noexcept;

    // Area of the loop polygon (shoelace); positive = CCW.
    [[nodiscard]] double signedArea() const noexcept;
};

// A trimmed region: one outer boundary loop + zero or more hole loops.
// The outer loop encloses the "keep" region; hole loops cut out from it.
struct TrimRegion {
    TrimLoop outer;
    std::vector<TrimLoop> holes;

    // True if point (u,v) is inside the outer loop and outside all holes.
    [[nodiscard]] bool contains(double u, double v) const noexcept;
};

struct TrimTessellationOptions {
    uint32_t samplesU    = 32;
    uint32_t samplesV    = 32;
    bool     keepInside  = true; // if false, keep the outside (invert trim)
};

class SurfaceTrimmer {
public:
    // Tessellate the surface and discard all sample quads whose centre falls
    // outside (or inside, if keepInside=false) every trim region.
    // Returns the trimmed Mesh.
    [[nodiscard]] static Mesh tessellate(
        const NurbsSurface& surface,
        const std::vector<TrimRegion>& regions,
        const TrimTessellationOptions& opts = {});

    // Build a rectangular trim region that covers the full parameter domain
    // [uMin, uMax] × [vMin, vMax].  Useful as the outer boundary.
    [[nodiscard]] static TrimRegion fullDomain(const NurbsSurface& surface);

    // Build a circular trim loop (approximated as a polyline with `segments`
    // steps) centred at (cu, cv) with radius r in parameter space.
    [[nodiscard]] static TrimLoop circularLoop(double cu, double cv,
                                               double r,
                                               uint32_t segments = 32);
};

} // namespace nexus::geometry
