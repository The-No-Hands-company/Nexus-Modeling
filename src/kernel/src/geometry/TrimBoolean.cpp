#include <nexus/geometry/TrimBoolean.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace nexus::geometry {

// ─── TrimBooleanOp ────────────────────────────────────────────────────────────

TrimBooleanOp::TrimBooleanOp(const TrimRegion& a,
                              const TrimRegion& b,
                              TrimBooleanType type)
    : m_a(&a), m_b(&b), m_type(type) {}

bool TrimBooleanOp::contains(double u, double v) const noexcept
{
    const bool inA = m_a->contains(u, v);
    const bool inB = m_b->contains(u, v);
    switch (m_type) {
        case TrimBooleanType::Union:        return inA || inB;
        case TrimBooleanType::Intersection: return inA && inB;
        case TrimBooleanType::Difference:   return inA && !inB;
    }
    return false;
}

// ─── helpers ─────────────────────────────────────────────────────────────────

// Compute the axis-aligned bounding rectangle of a TrimLoop's points.
static void loopBounds(const TrimLoop& loop,
                       double& uLo, double& uHi,
                       double& vLo, double& vHi)
{
    uLo = vLo =  std::numeric_limits<double>::max();
    uHi = vHi = -std::numeric_limits<double>::max();
    for (const auto& seg : loop.segments) {
        for (const auto& p : seg.points) {
            uLo = std::min(uLo, p.u); uHi = std::max(uHi, p.u);
            vLo = std::min(vLo, p.v); vHi = std::max(vHi, p.v);
        }
    }
}

// ─── TrimBooleanOp::toExplicitRegion ─────────────────────────────────────────

TrimRegion TrimBooleanOp::toExplicitRegion(
    double uLo, double uHi, double vLo, double vHi,
    uint32_t gridU, uint32_t gridV) const
{
    if (gridU < 2) gridU = 2;
    if (gridV < 2) gridV = 2;

    // Build a boolean occupancy grid.
    // cell (i, j): centre at uLo + (i+0.5)/(gridU) * (uHi-uLo).
    const uint32_t nU = gridU, nV = gridV;
    std::vector<bool> occupied(nU * nV, false);
    for (uint32_t i = 0; i < nU; ++i) {
        const double u = uLo + (uHi - uLo) * ((i + 0.5) / nU);
        for (uint32_t j = 0; j < nV; ++j) {
            const double v = vLo + (vHi - vLo) * ((j + 0.5) / nV);
            occupied[i * nV + j] = this->contains(u, v);
        }
    }

    // Represent the result as a TrimRegion with:
    //   outer  = bounding rectangle
    //   holes  = cells that are NOT occupied (represented as unit cells).
    //
    // Rather than extracting an exact polygon boundary, we encode the occupancy
    // grid as an outer rectangle + a set of tiny square holes at every vacant
    // grid cell.  SurfaceTrimmer::tessellate() then tests each quad centre
    // against the resulting TrimRegion, giving the correct keep/discard result
    // at the tessellation resolution.

    TrimRegion result;

    // Outer loop: bounding rectangle.
    {
        TrimCurveSegment seg;
        seg.closed = true;
        seg.points = {{uLo, vLo}, {uHi, vLo}, {uHi, vHi}, {uLo, vHi}, {uLo, vLo}};
        result.outer.segments.push_back(std::move(seg));
    }

    // Holes: one tiny square hole per vacant cell.
    const double du = (uHi - uLo) / nU;
    const double dv = (vHi - vLo) / nV;
    const double eps = 1e-10; // inset so ray-cast is never ambiguous at boundary
    for (uint32_t i = 0; i < nU; ++i) {
        for (uint32_t j = 0; j < nV; ++j) {
            if (occupied[i * nV + j]) { continue; }
            // Vacant cell → punch a hole.
            const double u0 = uLo + i * du + eps;
            const double u1 = u0  + du - 2.0 * eps;
            const double v0 = vLo + j * dv + eps;
            const double v1 = v0  + dv - 2.0 * eps;
            TrimLoop hole;
            TrimCurveSegment seg;
            seg.closed = true;
            seg.points = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}, {u0, v0}};
            hole.segments.push_back(std::move(seg));
            result.holes.push_back(std::move(hole));
        }
    }

    return result;
}

// ─── helpers to derive bounding box from two regions ─────────────────────────

static void regionBounds(const TrimRegion& r,
                          double& uLo, double& uHi,
                          double& vLo, double& vHi)
{
    loopBounds(r.outer, uLo, uHi, vLo, vHi);
}

// ─── free-function helpers ────────────────────────────────────────────────────

TrimRegion trimUnion(const TrimRegion& a, const TrimRegion& b,
                      uint32_t gridU, uint32_t gridV)
{
    double uLoA, uHiA, vLoA, vHiA;
    double uLoB, uHiB, vLoB, vHiB;
    regionBounds(a, uLoA, uHiA, vLoA, vHiA);
    regionBounds(b, uLoB, uHiB, vLoB, vHiB);
    const double uLo = std::min(uLoA, uLoB), uHi = std::max(uHiA, uHiB);
    const double vLo = std::min(vLoA, vLoB), vHi = std::max(vHiA, vHiB);
    TrimBooleanOp op(a, b, TrimBooleanType::Union);
    return op.toExplicitRegion(uLo, uHi, vLo, vHi, gridU, gridV);
}

TrimRegion trimIntersection(const TrimRegion& a, const TrimRegion& b,
                              uint32_t gridU, uint32_t gridV)
{
    double uLoA, uHiA, vLoA, vHiA;
    double uLoB, uHiB, vLoB, vHiB;
    regionBounds(a, uLoA, uHiA, vLoA, vHiA);
    regionBounds(b, uLoB, uHiB, vLoB, vHiB);
    const double uLo = std::min(uLoA, uLoB), uHi = std::max(uHiA, uHiB);
    const double vLo = std::min(vLoA, vLoB), vHi = std::max(vHiA, vHiB);
    TrimBooleanOp op(a, b, TrimBooleanType::Intersection);
    return op.toExplicitRegion(uLo, uHi, vLo, vHi, gridU, gridV);
}

TrimRegion trimDifference(const TrimRegion& a, const TrimRegion& b,
                           uint32_t gridU, uint32_t gridV)
{
    double uLoA, uHiA, vLoA, vHiA;
    double uLoB, uHiB, vLoB, vHiB;
    regionBounds(a, uLoA, uHiA, vLoA, vHiA);
    regionBounds(b, uLoB, uHiB, vLoB, vHiB);
    const double uLo = std::min(uLoA, uLoB), uHi = std::max(uHiA, uHiB);
    const double vLo = std::min(vLoA, vLoB), vHi = std::max(vHiA, vHiB);
    TrimBooleanOp op(a, b, TrimBooleanType::Difference);
    return op.toExplicitRegion(uLo, uHi, vLo, vHi, gridU, gridV);
}

} // namespace nexus::geometry
