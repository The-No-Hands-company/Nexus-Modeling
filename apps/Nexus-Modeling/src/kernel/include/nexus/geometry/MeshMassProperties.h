#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

struct MassProperties {
    float volume = 0.f;
    nexus::render::Vec3 centroid = {0.f, 0.f, 0.f};
    float inertia[3][3] = {};
    float principalMoments[3] = {};
    nexus::render::Vec3 principalAxes[3] = {};
};

class MeshMassProperties {
public:
    [[nodiscard]] static MassProperties compute(const Mesh& mesh);
};

} // namespace nexus::geometry
