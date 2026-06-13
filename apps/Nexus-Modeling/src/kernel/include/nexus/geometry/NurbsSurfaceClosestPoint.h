#pragma once
// ─── Nexus Geometry ── NurbsSurfaceClosestPoint ─────────────────────────

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct NurbsSurfaceClosestPointOptions {
    int32_t gridSizeU    = 10;
    int32_t gridSizeV    = 10;
    int32_t maxIter      = 25;
    float   tolerance    = 1e-5f;
    float   stepDamping  = 0.5f;
    float   minStep      = 1e-6f;
};

struct NurbsSurfaceClosestPointResult {
    float u     = 0.f;
    float v     = 0.f;
    Vec3  point;
    float distance = 0.f;
    int32_t iterations = 0;
    bool converged = false;
};

class NurbsSurfaceClosestPoint {
public:
    static NurbsSurfaceClosestPointResult project(
        const NurbsSurface& surface,
        const Vec3& query,
        const NurbsSurfaceClosestPointOptions& opts = {});
};

} // namespace nexus::geometry
