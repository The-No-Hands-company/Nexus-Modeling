#pragma once

#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct HausdorffOptions {
    uint32_t sampleCount = 4096;
    uint32_t seed = 12345;
};

struct HausdorffResult {
    float forwardMax = 0.f;
    float backwardMax = 0.f;
    float symmetricMax = 0.f;
    float forwardMean = 0.f;
    float backwardMean = 0.f;
    float symmetricMean = 0.f;
};

class MeshHausdorffDistance {
public:
    [[nodiscard]] static HausdorffResult compute(
        const Mesh& a, const Mesh& b,
        const HausdorffOptions& opts = {});
};

} // namespace nexus::geometry
