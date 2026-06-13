#pragma once
// ─── Nexus Geometry ── NurbsCurveClosestPoint ───────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct NurbsCurveClosestPointOptions {
    int32_t samples      = 50;
    int32_t maxIter      = 20;
    float   tolerance    = 1e-5f;
    float   stepDamping  = 0.5f;
    float   minStep      = 1e-7f;
};

struct NurbsCurveClosestPointResult {
    float param     = 0.f;
    Vec3  point;
    float distance  = 0.f;
    int32_t iterations = 0;
    bool   converged = false;
};

class NurbsCurveClosestPoint {
public:
    static NurbsCurveClosestPointResult project(
        const NurbsCurve& curve,
        const Vec3& query,
        const NurbsCurveClosestPointOptions& opts = {});
};

} // namespace nexus::geometry
