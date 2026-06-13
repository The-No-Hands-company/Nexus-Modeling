#pragma once
// ── Nexus Geometry — GeodesicPath

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

class GeodesicPath {
public:
    using Vec3 = nexus::render::Vec3;

    struct Result {
        std::vector<Vec3> path3D;
        float arcLength = 0.f;
    };

    [[nodiscard]] static Result compute(const NurbsSurface& surface,
                                         float u0, float v0,
                                         float u1, float v1,
                                         int32_t gridRes = 64);
};

} // namespace nexus::geometry
