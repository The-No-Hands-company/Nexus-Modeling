#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — 3D Delaunay Tetrahedralization
//
//  Incremental Bowyer-Watson algorithm for constructing the Delaunay
//  tetrahedralization of a 3D point set.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/render/Camera.h>

#include <array>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct TetMesh {
    std::vector<Vec3>                     vertices;
    std::vector<std::array<uint32_t, 4>> tetrahedra;  // 4 vertex indices per tet

    [[nodiscard]] size_t vertexCount()      const noexcept { return vertices.size(); }
    [[nodiscard]] size_t tetrahedronCount() const noexcept { return tetrahedra.size(); }
    [[nodiscard]] bool   empty()            const noexcept { return tetrahedra.empty(); }
};

struct TetDelaunayOptions {
    float epsilon = 1e-10f;
};

class TetDelaunay3D {
public:
    [[nodiscard]] static TetMesh compute(const std::vector<Vec3>& points,
                                          const TetDelaunayOptions& opts = {}) noexcept;
};

} // namespace nexus::geometry
