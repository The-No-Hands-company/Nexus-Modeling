#pragma once

#include <nexus/geometry/Mesh.h>

#include <array>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct VoxelizeOptions {
    std::array<uint32_t, 3> resolution = {32, 32, 32};
    nexus::render::Vec3 origin = {0.f, 0.f, 0.f};
    nexus::render::Vec3 voxelSize = {1.f, 1.f, 1.f};
};

struct VoxelGrid {
    std::array<uint32_t, 3> resolution;
    nexus::render::Vec3 origin;
    nexus::render::Vec3 voxelSize;
    std::vector<bool> occupancy;

    [[nodiscard]] size_t index(uint32_t x, uint32_t y, uint32_t z) const noexcept {
        return static_cast<size_t>(z) * resolution[0] * resolution[1]
             + static_cast<size_t>(y) * resolution[0] + x;
    }

    [[nodiscard]] bool get(uint32_t x, uint32_t y, uint32_t z) const noexcept {
        return occupancy[index(x, y, z)];
    }
};

class MeshVoxelize {
public:
    [[nodiscard]] static VoxelGrid voxelize(const Mesh& mesh, const VoxelizeOptions& opts = {});
};

} // namespace nexus::geometry
