#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

struct ThickenOptions {
    float thickness = 0.1f;
    bool closedSolid = true;
};

class MeshThicken {
public:
    [[nodiscard]] static Mesh solidify(const Mesh& mesh, const ThickenOptions& opts = {});
};

} // namespace nexus::geometry
