#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct PoissonDiskOptions {
    float minDist = 0.1f;
    uint32_t maxAttempts = 30;
    uint32_t maxPoints = 0;
    uint64_t seed = 42;
};

struct PoissonDiskResult {
    std::vector<nexus::render::Vec3> points;
    std::vector<nexus::render::Vec3> normals;
    std::vector<uint32_t> triangles;
};

class MeshPoissonDisk {
public:
    [[nodiscard]] static PoissonDiskResult sample(
        const Mesh& mesh,
        const PoissonDiskOptions& opts = {});
};

} // namespace nexus::geometry
