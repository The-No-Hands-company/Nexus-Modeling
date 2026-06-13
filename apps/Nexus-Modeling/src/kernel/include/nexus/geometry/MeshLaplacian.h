#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct SmoothOptions {
    float lambda = 0.5f;
    uint32_t iterations = 10;
    bool recomputeNormals = true;
    bool fixBoundary = false;
};

class MeshLaplacian {
public:
    [[nodiscard]] static Mesh smooth(const Mesh& mesh, const SmoothOptions& opts = {});
    [[nodiscard]] static std::vector<float> meanCurvature(const Mesh& mesh);
};

} // namespace nexus::geometry
