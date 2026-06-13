#pragma once
// ─── Nexus Geometry ── SurfaceCurvature ────────────────────────────────

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct CurvatureSample {
    float u     = 0.f;
    float v     = 0.f;
    Vec3  point;
    Vec3  normal;
    float gaussianCurvature = 0.f;
    float meanCurvature     = 0.f;
    float principalK1       = 0.f;
    float principalK2       = 0.f;
};

class SurfaceCurvature {
public:
    static std::vector<CurvatureSample> sample(
        const NurbsSurface& surface,
        int32_t samplesU, int32_t samplesV);

private:
    static void computeAt(const NurbsSurface& surface,
                          float u, float v, CurvatureSample& out);
};

} // namespace nexus::geometry
