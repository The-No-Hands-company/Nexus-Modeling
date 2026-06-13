#pragma once

#include <nexus/geometry/Mesh.h>

#include <array>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct ConvexHull {
    std::vector<nexus::render::Vec3> vertices;
    std::vector<std::array<uint32_t, 3>> faces;
};

class MeshConvexHull {
public:
    [[nodiscard]] static ConvexHull build(const std::vector<nexus::render::Vec3>& points);
    [[nodiscard]] static ConvexHull fromMesh(const Mesh& mesh);
};

} // namespace nexus::geometry
