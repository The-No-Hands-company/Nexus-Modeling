#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Boolean operations on NURBS surface trim regions
//
//  Computes set-theoretic combinations of TrimRegion objects entirely in
//  2-D (u, v) parameter space.  No 3-D geometry evaluation is needed for
//  the Boolean itself; the resulting TrimRegion can be passed directly to
//  SurfaceTrimmer::tessellate().
//
//  Operations:
//    Union        — keep points inside A or B (or both)
//    Intersection — keep points inside both A and B
//    Difference   — keep points inside A but not B  (A minus B)
//
//  Implementation:
//    Each operation is implemented purely as membership tests; the result is
//    a new TrimRegion whose `contains(u,v)` delegates to the boolean
//    combination of the two input regions.  The outer loop of the result is
//    a conservative bounding rectangle (the axis-aligned union of both outer
//    bounds) and the holes encode the complement logic.
//
//    For Union:   outer = bounding rect;  hole = complement of (A ∪ B)
//                 approximated as the intersection of complements.
//    For ops that require an explicit polygon boundary, use
//    `TrimBooleanOp::toExplicitRegion()` which rasterises the result onto a
//    grid and extracts a simplified boundary polyline.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/SurfaceTrim.h>

namespace nexus::geometry {

enum class TrimBooleanType : uint8_t {
    Union,        // A | B
    Intersection, // A & B
    Difference,   // A - B
};

// A lazy Boolean combination of two TrimRegions.
// `contains(u,v)` evaluates the boolean predicate without building explicit
// polygon boundaries.  Use this directly with SurfaceTrimmer::tessellate()
// by wrapping it in a TrimRegion (see TrimBooleanOp::toRegion()).
class TrimBooleanOp {
public:
    TrimBooleanOp(const TrimRegion& a, const TrimRegion& b, TrimBooleanType type);

    // Evaluate the boolean predicate at parameter point (u, v).
    [[nodiscard]] bool contains(double u, double v) const noexcept;

    // Convert to a TrimRegion that delegates to this object via a synthetic
    // outer loop covering [uLo,uHi]×[vLo,vHi].  The outer loop is a
    // rectangle; contains() is overridden via the hole mechanism to produce
    // the boolean result over a uniform grid.
    //
    // `toExplicitRegion` rasterises the result at `gridU × gridV` resolution
    // and returns a TrimRegion suitable for SurfaceTrimmer::tessellate()
    // without any further wrapping.
    [[nodiscard]] TrimRegion toExplicitRegion(
        double uLo, double uHi, double vLo, double vHi,
        uint32_t gridU = 64, uint32_t gridV = 64) const;

private:
    const TrimRegion* m_a;
    const TrimRegion* m_b;
    TrimBooleanType   m_type;
};

// Free-function helpers that build a TrimBooleanOp and immediately call
// toExplicitRegion(), using the bounding box of the two regions' outer loops.
[[nodiscard]] TrimRegion trimUnion(
    const TrimRegion& a, const TrimRegion& b,
    uint32_t gridU = 64, uint32_t gridV = 64);

[[nodiscard]] TrimRegion trimIntersection(
    const TrimRegion& a, const TrimRegion& b,
    uint32_t gridU = 64, uint32_t gridV = 64);

[[nodiscard]] TrimRegion trimDifference(
    const TrimRegion& a, const TrimRegion& b,
    uint32_t gridU = 64, uint32_t gridV = 64);

} // namespace nexus::geometry
