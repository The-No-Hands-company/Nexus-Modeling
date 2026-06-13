#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

class MeshIslandDecomposition {
public:
    [[nodiscard]] static std::vector<Mesh> decompose(const Mesh& mesh);
    [[nodiscard]] static size_t count(const Mesh& mesh);
};

} // namespace nexus::geometry
