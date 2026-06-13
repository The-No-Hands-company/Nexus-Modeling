#pragma once
// --- Nexus Geometry — Delaunay2D

#include <nexus/geometry/Mesh.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

struct DelaunayResult {
    bool ok = false;
    std::string error;
    std::vector<std::array<uint32_t, 3>> triangles;
    std::vector<Vec2> vertices;
};

class Delaunay2D {
public:
    [[nodiscard]] static DelaunayResult triangulate(const std::vector<Vec2>& points);
};

} // namespace nexus::geometry
