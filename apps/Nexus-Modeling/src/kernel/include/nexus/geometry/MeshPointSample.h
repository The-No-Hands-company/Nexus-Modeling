#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct PointSampleOptions {
    uint32_t count = 1024;
    uint64_t seed = 42;
};

struct PointSampleResult {
    std::vector<nexus::render::Vec3> points;
    std::vector<nexus::render::Vec3> normals;
    std::vector<uint32_t> triangles;
};

class MeshPointSample {
public:
    [[nodiscard]] static PointSampleResult sample(
        const Mesh& mesh,
        const PointSampleOptions& opts = {});
};

} // namespace nexus::geometry
