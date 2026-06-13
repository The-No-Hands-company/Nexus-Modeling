#pragma once
// ── Nexus Geometry — SurfaceCurveProjection

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

class SurfaceCurveProjection {
public:
    using Vec3 = nexus::render::Vec3;

    struct UVParam {
        float u = 0.f;
        float v = 0.f;
    };

    struct Result {
        std::vector<UVParam> params;
        std::vector<Vec3> projectedPoints;
        float maxError = 0.f;
    };

    [[nodiscard]] static Result projectCurve(
        const NurbsSurface& surface,
        const NurbsCurve& curve,
        int32_t samples = 128);
};

} // namespace nexus::geometry
