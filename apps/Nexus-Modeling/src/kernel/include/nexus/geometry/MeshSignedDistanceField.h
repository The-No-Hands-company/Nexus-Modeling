#pragma once

#include <nexus/geometry/Mesh.h>

#include <array>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct SDFOptions {
    std::array<uint32_t, 3> resolution = {32, 32, 32};
    nexus::render::Vec3 origin = {0.f, 0.f, 0.f};
    nexus::render::Vec3 cellSize = {1.f, 1.f, 1.f};
};

struct SDFResult {
    std::array<uint32_t, 3> resolution;
    nexus::render::Vec3 origin;
    nexus::render::Vec3 cellSize;
    std::vector<float> distances;
};

class MeshSignedDistanceField {
public:
    [[nodiscard]] static SDFResult compute(const Mesh& mesh, const SDFOptions& opts = {});
};

} // namespace nexus::geometry
