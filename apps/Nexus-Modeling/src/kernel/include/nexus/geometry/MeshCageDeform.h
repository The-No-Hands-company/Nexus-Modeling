#pragma once

#include <nexus/geometry/Mesh.h>

#include <array>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct FFDGrid {
    std::array<uint32_t, 3> resolution = {4, 4, 4};
    nexus::render::Vec3 origin = {0.f, 0.f, 0.f};
    nexus::render::Vec3 cellSize = {1.f, 1.f, 1.f};
    std::vector<nexus::render::Vec3> controlPoints;
};

class MeshCageDeform {
public:
    [[nodiscard]] Mesh bind(const Mesh& mesh, const FFDGrid& grid);
    [[nodiscard]] Mesh deform(const Mesh& mesh, const std::vector<nexus::render::Vec3>& deformedCagePoints);

private:
    std::vector<std::array<uint32_t, 8>> m_bindingIndices;
    std::vector<std::array<float, 8>>    m_bindingWeights;
};

} // namespace nexus::geometry
