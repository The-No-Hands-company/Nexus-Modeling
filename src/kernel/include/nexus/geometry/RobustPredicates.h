#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — robust geometric predicates (Shewchuk adaptive arithmetic)
//
//  Provides exact-sign orientation tests via a two-stage pipeline:
//    1. Fast floating-point filter with Shewchuk error bounds.
//    2. Adaptive exact expansion arithmetic when the filter is inconclusive.
//
//  Return value convention: positive, zero, or negative (not normalised to ±1).
//    orient2d > 0  →  pa, pb, pc are counter-clockwise
//    orient2d < 0  →  pa, pb, pc are clockwise
//    orient2d = 0  →  pa, pb, pc are collinear
//
//    orient3d > 0  →  pd is below the plane defined by pa, pb, pc (CCW when
//                     viewed from above)
//    orient3d < 0  →  pd is above that plane
//    orient3d = 0  →  pa, pb, pc, pd are coplanar
// ─────────────────────────────────────────────────────────────────────────────

namespace nexus::geometry::predicates {

// 2-D orientation determinant.
// pa, pb, pc are arrays of two doubles: {x, y}.
[[nodiscard]] double orient2d(const double pa[2],
                              const double pb[2],
                              const double pc[2]) noexcept;

// 3-D orientation determinant.
// pa, pb, pc, pd are arrays of three doubles: {x, y, z}.
[[nodiscard]] double orient3d(const double pa[3],
                              const double pb[3],
                              const double pc[3],
                              const double pd[3]) noexcept;

} // namespace nexus::geometry::predicates
