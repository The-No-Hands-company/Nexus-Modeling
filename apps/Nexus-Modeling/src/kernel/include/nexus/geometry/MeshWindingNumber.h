#pragma once

#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct WindingNumberOptions {
    float epsilon = 1e-6f;
    float isInsideThreshold = 0.5f;
};

class MeshWindingNumber {
public:
    [[nodiscard]] static float at(const Mesh& mesh,
                                   const nexus::render::Vec3& query);

    [[nodiscard]] static std::vector<float> batch(
        const Mesh& mesh,
        const std::vector<nexus::render::Vec3>& queries);

    [[nodiscard]] static bool isInside(const Mesh& mesh,
                                        const nexus::render::Vec3& query,
                                        const WindingNumberOptions& opts = {});
};

} // namespace nexus::geometry
