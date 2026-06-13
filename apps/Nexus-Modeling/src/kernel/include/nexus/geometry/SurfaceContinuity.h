#pragma once
// ─── Nexus Geometry ── SurfaceContinuity ───────────────────────────────

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

enum class SurfaceEdge { UMin, UMax, VMin, VMax };

struct SurfaceContinuityOptions {
    int32_t samples = 100;
};

struct SurfaceContinuityResult {
    float maxPositionalGap  = 0.f;
    float maxAngularDevDeg  = 0.f;
    float avgPositionalGap  = 0.f;
    float avgAngularDevDeg  = 0.f;
    bool  g0Continuous      = false;
    bool  g1Continuous      = false;
    std::vector<float> positionalGaps;
    std::vector<float> angularDevs;
    std::vector<float> sampleParams;
};

class SurfaceContinuity {
public:
    static SurfaceContinuityResult analyze(
        const NurbsSurface& surfaceA, SurfaceEdge edgeA,
        const NurbsSurface& surfaceB, SurfaceEdge edgeB,
        const SurfaceContinuityOptions& opts = {});
};

} // namespace nexus::geometry
