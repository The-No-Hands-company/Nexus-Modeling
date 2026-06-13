#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct HarmonicConstraints {
    std::vector<uint32_t> constrainedVertices;
    std::vector<float> constrainedValues;
};

struct HarmonicOptions {
    float omega = 1.0f;
    uint32_t maxIterations = 1000;
    float tolerance = 1e-6f;
};

class MeshHarmonicField {
public:
    [[nodiscard]] static std::vector<float> solve(const Mesh& mesh,
                                                   const HarmonicConstraints& constraints,
                                                   const HarmonicOptions& opts = {});
};

} // namespace nexus::geometry
