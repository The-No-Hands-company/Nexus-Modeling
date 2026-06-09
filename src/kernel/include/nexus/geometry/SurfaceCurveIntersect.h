#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — NURBS surface / curve intersection
//
//  Finds points where a 3-D NURBS curve pierces a NURBS surface, i.e.
//  parameter pairs (t, u, v) such that  C(t) == S(u, v).
//
//  Algorithm  (Newton-Raphson root finding):
//    1. Coarse grid search: sample the surface on a (gridU × gridV) mesh and
//       the curve at `gridT` parameter values.  For each (t_i, u_j, v_k)
//       triple evaluate the squared distance ‖C(t) − S(u,v)‖².  Collect
//       triples whose distance is below a loose threshold as seed candidates.
//    2. Newton refinement: for each seed iterate the 3×3 system
//         F(t, u, v) = C(t) − S(u, v) = 0
//       using the Jacobian [C'(t), −Su(u,v), −Sv(u,v)] until convergence
//       (|F| < tolerance) or max iterations.
//    3. De-duplicate results that converge to the same parameter triple.
//
//  Limitations:
//    - Only transverse intersections are robustly detected; near-tangential
//      contacts may be missed or duplicated.
//    - Curves entirely lying on the surface are not handled.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>

#include <vector>

namespace nexus::geometry {

struct SurfaceCurveIsect {
    double t;           // curve parameter
    double u;           // surface parameter U
    double v;           // surface parameter V
    nexus::render::Vec3 point; // 3-D intersection point (average of C(t) and S(u,v))
};

struct SurfaceCurveIsectOptions {
    uint32_t gridT      = 16;   // coarse curve samples
    uint32_t gridU      = 16;   // coarse surface U samples
    uint32_t gridV      = 16;   // coarse surface V samples
    float    tolerance  = 1e-5f;// convergence threshold for |F|
    uint32_t maxIter    = 32;   // Newton max iterations per seed
};

class SurfaceCurveIntersector {
public:
    // Find all intersections of `curve` with `surface`.
    // Returns a list of intersection records sorted by curve parameter t.
    [[nodiscard]] static std::vector<SurfaceCurveIsect> intersect(
        const NurbsCurve&   curve,
        const NurbsSurface& surface,
        const SurfaceCurveIsectOptions& opts = {});
};

} // namespace nexus::geometry
