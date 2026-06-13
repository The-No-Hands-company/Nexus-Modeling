#pragma once
// --- Nexus Geometry — Meshlet

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

struct NormalCone {
    nexus::render::Vec3 axis        = {0.f, 0.f, 1.f};
    float              angleCutoff  = -1.f;
    nexus::render::Vec3 apexOffset  = {0.f, 0.f, 0.f};
};

struct MeshletData {
    std::vector<uint32_t> vertexIndices;
    std::vector<uint32_t> triangleIndices;
    std::vector<uint8_t> primitiveIndices;
    uint32_t vertexCount = 0, triangleCount = 0;
    NormalCone normalCone;
    nexus::render::Aabb aabb;
};

struct MeshletResult {
    bool ok = false;
    std::string error;
    std::vector<MeshletData> meshlets;
};

class MeshletBuilder {
public:
    [[nodiscard]] static MeshletResult build(const Mesh& mesh, uint32_t maxVerts = 64,
                                              uint32_t maxTris = 126);
};

} // namespace nexus::geometry
