#pragma once
// ── Nexus Geometry — SurfaceSurfaceIntersect

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct IntersectSeedGrid {
    int32_t resU = 16;
    int32_t resV = 16;
};

class SurfaceSurfaceIntersect {
public:
    using Vec3 = nexus::render::Vec3;

    [[nodiscard]] static std::vector<std::vector<Vec3>> intersect(
        const NurbsSurface& a,
        const NurbsSurface& b,
        const IntersectSeedGrid& seedGrid = {},
        int32_t marchSteps = 100,
        float stepSize = 0.01f);
};

} // namespace nexus::geometry
