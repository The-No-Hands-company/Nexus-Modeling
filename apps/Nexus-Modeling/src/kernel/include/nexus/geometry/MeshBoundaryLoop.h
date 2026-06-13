#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct BoundaryLoopResult {
    std::vector<std::vector<uint32_t>> loops;
    std::vector<float> perimeters;
};

class MeshBoundaryLoop {
public:
    [[nodiscard]] static BoundaryLoopResult extract(const Mesh& mesh);
};

} // namespace nexus::geometry
