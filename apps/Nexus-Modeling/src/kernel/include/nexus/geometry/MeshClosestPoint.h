#pragma once

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBVH.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace nexus::geometry {

struct MeshClosestHit {
    float distanceSquared = std::numeric_limits<float>::max();
    nexus::render::Vec3 point;
    uint32_t triangle = 0;
    float baryU = 0.f;
    float baryV = 0.f;
};

struct MeshClosestBatch {
    std::vector<MeshClosestHit> hits;
};

class MeshClosestPoint {
public:
    explicit MeshClosestPoint(const Mesh& mesh);

    [[nodiscard]] MeshClosestHit closest(const nexus::render::Vec3& query) const;
    [[nodiscard]] MeshClosestBatch closestBatch(const std::vector<nexus::render::Vec3>& queries) const;

    [[nodiscard]] static MeshClosestHit closest(const Mesh& mesh,
                                                 const nexus::render::Vec3& query);
    [[nodiscard]] static MeshClosestBatch query(
        const Mesh& mesh,
        const std::vector<nexus::render::Vec3>& queries);

private:
    MeshBVH m_bvh;
    const Mesh* m_mesh = nullptr;
};

} // namespace nexus::geometry
