#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace nexus::geometry {

struct DisplaceOptions {
    float magnitude = 1.f;
    bool recomputeNormals = true;
};

class MeshDisplace {
public:
    [[nodiscard]] static Mesh displaceByScalar(
        const Mesh& mesh,
        const std::function<float(const nexus::render::Vec3&)>& scalarField,
        const DisplaceOptions& opts = {});

    [[nodiscard]] static Mesh displaceByVector(
        const Mesh& mesh,
        const std::function<nexus::render::Vec3(const nexus::render::Vec3&)>& vectorField,
        const DisplaceOptions& opts = {});
};

} // namespace nexus::geometry
