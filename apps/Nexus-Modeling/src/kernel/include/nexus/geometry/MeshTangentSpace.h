#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

class MeshTangentSpace {
public:
    [[nodiscard]] static Mesh compute(const Mesh& mesh);
};

} // namespace nexus::geometry
