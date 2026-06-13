#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct AmbientOcclusionOptions {
    uint32_t sampleCount = 64;
    float maxDistance = 1.f;
    uint64_t seed = 12345;
    bool useBVH = true;
    uint32_t surfaceSampleCount = 0;
};

struct AmbientOcclusionResult {
    std::vector<float> ao;
    std::vector<nexus::render::Vec3> bentNormals;
};

class MeshAmbientOcclusion {
public:
    [[nodiscard]] static AmbientOcclusionResult bake(
        const Mesh& mesh,
        const AmbientOcclusionOptions& opts = {});
};

} // namespace nexus::geometry
