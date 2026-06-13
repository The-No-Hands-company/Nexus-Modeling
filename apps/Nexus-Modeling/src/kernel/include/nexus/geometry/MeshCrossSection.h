#pragma once
// ── Nexus Geometry — MeshCrossSection

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

class MeshCrossSection {
public:
    using Vec3 = nexus::render::Vec3;

    struct Contour {
        std::vector<Vec3> points;
        bool closed = false;
    };

    [[nodiscard]] static std::vector<Contour> slice(
        const Mesh& mesh,
        const Vec3& origin,
        const Vec3& normal);
};

} // namespace nexus::geometry
