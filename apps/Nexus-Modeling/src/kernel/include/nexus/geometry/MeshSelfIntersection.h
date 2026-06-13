#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct SelfIntersectionPair {
    uint32_t faceA = 0;
    uint32_t faceB = 0;
};

struct SelfIntersectionResult {
    std::vector<SelfIntersectionPair> intersections;
};

class MeshSelfIntersection {
public:
    [[nodiscard]] static SelfIntersectionResult detect(const Mesh& mesh);
};

} // namespace nexus::geometry
