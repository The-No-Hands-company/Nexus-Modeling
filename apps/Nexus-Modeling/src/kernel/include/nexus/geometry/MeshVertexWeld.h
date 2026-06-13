#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

struct WeldOptions {
    float tolerance = 1e-5f;
};

class MeshVertexWeld {
public:
    [[nodiscard]] static Mesh weld(const Mesh& mesh, const WeldOptions& opts = {});
    [[nodiscard]] static size_t countUnique(const Mesh& mesh, float tolerance = 1e-5f);
};

} // namespace nexus::geometry
