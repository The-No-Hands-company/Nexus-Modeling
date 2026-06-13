#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct InstancerOptions {
    float scaleMin = 0.5f;
    float scaleMax = 1.5f;
    uint64_t seed = 42;
    bool alignToNormal = true;
};

class MeshInstancer {
public:
    [[nodiscard]] static Mesh scatter(
        const Mesh& source,
        const Mesh& target,
        const InstancerOptions& opts = {});

    [[nodiscard]] static Mesh scatterAtPoints(
        const Mesh& source,
        const std::vector<nexus::render::Vec3>& positions,
        const std::vector<nexus::render::Vec3>& normals,
        const InstancerOptions& opts = {});
};

} // namespace nexus::geometry
