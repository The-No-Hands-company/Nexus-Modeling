#include <nexus/geometry/SurfaceCurveIntersect.h>

#include <algorithm>
#include <cmath>

namespace nexus::geometry {

// ─── 3×3 linear solve (Cramer's rule) ────────────────────────────────────────

// Solve A·x = b where A is column-major (col0, col1, col2).
// Returns false if det ≈ 0.
static bool solve3x3(
    float a0, float a1, float a2,  // col0
    float b0, float b1, float b2,  // col1
    float c0, float c1, float c2,  // col2
    float r0, float r1, float r2,  // rhs
    float& x, float& y, float& z)
{
    const float det = a0*(b1*c2 - b2*c1)
                    - a1*(b0*c2 - b2*c0)
                    + a2*(b0*c1 - b1*c0);
    if (std::abs(det) < 1e-15f) { return false; }
    const float inv = 1.f / det;
    x = inv * (r0*(b1*c2 - b2*c1) - r1*(b0*c2 - b2*c0) + r2*(b0*c1 - b1*c0));
    y = inv * (a0*(r1*c2 - r2*c1) - a1*(r0*c2 - r2*c0) + a2*(r0*c1 - r1*c0));
    z = inv * (a0*(b1*r2 - b2*r1) - a1*(b0*r2 - b2*r0) + a2*(b0*r1 - b1*r0));
    return true;
}

// ─── SurfaceCurveIntersector::intersect ──────────────────────────────────────

std::vector<SurfaceCurveIsect> SurfaceCurveIntersector::intersect(
    const NurbsCurve&   curve,
    const NurbsSurface& surface,
    const SurfaceCurveIsectOptions& opts)
{
    const double tLo = curve.paramMin(),   tHi = curve.paramMax();
    const double uLo = surface.paramMinU(), uHi = surface.paramMaxU();
    const double vLo = surface.paramMinV(), vHi = surface.paramMaxV();

    const uint32_t gT = std::max(opts.gridT, 2u);
    const uint32_t gU = std::max(opts.gridU, 2u);
    const uint32_t gV = std::max(opts.gridV, 2u);

    // ── Coarse grid search ───────────────────────────────────────────────────
    struct Seed { double t, u, v; };
    std::vector<Seed> seeds;

    // Coarse threshold: 4× grid cell diagonal so seeds are found across grid
    // spacing regardless of the fine Newton tolerance.
    const double duCell = (uHi - uLo) / (gU > 1 ? gU - 1 : 1);
    const double dvCell = (vHi - vLo) / (gV > 1 ? gV - 1 : 1);
    const double dtCell = (tHi - tLo) / (gT > 1 ? gT - 1 : 1);
    // Estimate max 3-D distance between adjacent grid samples by evaluating
    // the surface diagonal step size using a representative cell step.
    // As a conservative bound use: 4 × (duCell + dvCell + dtCell).
    const float looseTol  = static_cast<float>(4.0 * (duCell + dvCell + dtCell));
    const float looseTol2 = looseTol * looseTol;

    // Sample curve at gT points.
    std::vector<nexus::render::Vec3> cPts(gT);
    std::vector<double> cT(gT);
    for (uint32_t ti = 0; ti < gT; ++ti) {
        cT[ti]  = tLo + (tHi - tLo) * (static_cast<double>(ti) / (gT - 1));
        cPts[ti] = curve.evaluate(cT[ti]);
    }

    // Sample surface on gU×gV grid.
    for (uint32_t ui = 0; ui < gU; ++ui) {
        const double u = uLo + (uHi - uLo) * (static_cast<double>(ui) / (gU - 1));
        for (uint32_t vi = 0; vi < gV; ++vi) {
            const double v = vLo + (vHi - vLo) * (static_cast<double>(vi) / (gV - 1));
            const auto sp = surface.evaluate(u, v);
            for (uint32_t ti = 0; ti < gT; ++ti) {
                const float dx = cPts[ti].x - sp.x;
                const float dy = cPts[ti].y - sp.y;
                const float dz = cPts[ti].z - sp.z;
                if (dx*dx + dy*dy + dz*dz < looseTol2) {
                    seeds.push_back({cT[ti], u, v});
                }
            }
        }
    }

    if (seeds.empty()) { return {}; }

    // ── Newton refinement ────────────────────────────────────────────────────
    const float tol2 = opts.tolerance * opts.tolerance;
    std::vector<SurfaceCurveIsect> results;

    for (auto& s : seeds) {
        double t = s.t, u = s.u, v = s.v;
        bool converged = false;

        for (uint32_t iter = 0; iter < opts.maxIter; ++iter) {
            const auto Ct = curve.evaluate(t);
            const auto Su = surface.evaluate(u, v);
            const float fx = Ct.x - Su.x;
            const float fy = Ct.y - Su.y;
            const float fz = Ct.z - Su.z;
            const float f2 = fx*fx + fy*fy + fz*fz;

            if (f2 < tol2) { converged = true; break; }

            // Jacobian columns: dF/dt = C'(t), dF/du = -dS/du, dF/dv = -dS/dv
            const auto Ct_d  = curve.derivative1(t);
            const auto Su_du = surface.derivU(u, v);
            const auto Su_dv = surface.derivV(u, v);

            float dt, du, dv;
            if (!solve3x3(
                    Ct_d.x, Ct_d.y, Ct_d.z,
                    -Su_du.x, -Su_du.y, -Su_du.z,
                    -Su_dv.x, -Su_dv.y, -Su_dv.z,
                    -fx, -fy, -fz,
                    dt, du, dv)) { break; }

            t = std::clamp(t + dt, tLo, tHi);
            u = std::clamp(u + du, uLo, uHi);
            v = std::clamp(v + dv, vLo, vHi);
        }

        if (!converged) { continue; }

        // Check for duplicates (same parameter triple within tolerance).
        const auto Ct = curve.evaluate(t);
        bool dup = false;
        for (const auto& r : results) {
            const float dt2 = static_cast<float>(t - r.t);
            if (dt2*dt2 < tol2) { dup = true; break; }
        }
        if (!dup) {
            const auto Su = surface.evaluate(u, v);
            nexus::render::Vec3 mid{(Ct.x + Su.x) * 0.5f,
                                    (Ct.y + Su.y) * 0.5f,
                                    (Ct.z + Su.z) * 0.5f};
            results.push_back({t, u, v, mid});
        }
    }

    std::sort(results.begin(), results.end(),
        [](const SurfaceCurveIsect& a, const SurfaceCurveIsect& b) {
            return a.t < b.t;
        });

    return results;
}

} // namespace nexus::geometry
