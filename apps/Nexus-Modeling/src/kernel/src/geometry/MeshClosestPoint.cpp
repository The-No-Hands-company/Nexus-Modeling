#include <nexus/geometry/MeshClosestPoint.h>

#include <algorithm>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

MeshClosestPoint::MeshClosestPoint(const Mesh& mesh)
    : m_mesh(&mesh)
{
    if (mesh.isValid()) {
        m_bvh.build(mesh);
    }
}

MeshClosestHit MeshClosestPoint::closest(const Vec3& query) const {
    MeshClosestHit result;
    if (!m_mesh || !m_mesh->isValid() || !m_bvh.isValid()) return result;

    auto hit = m_bvh.closestPoint(query);
    if (!hit.valid) return result;

    result.distanceSquared = hit.distanceSquared;
    result.triangle = hit.triangleIndex;
    result.baryU = hit.baryU;
    result.baryV = hit.baryV;
    result.point = hit.point;
    return result;
}

MeshClosestBatch MeshClosestPoint::closestBatch(const std::vector<Vec3>& queries) const {
    MeshClosestBatch batch;
    if (!m_mesh || !m_mesh->isValid() || !m_bvh.isValid() || queries.empty()) return batch;

    batch.hits.reserve(queries.size());
    for (const auto& q : queries) {
        auto hit = m_bvh.closestPoint(q);
        MeshClosestHit h;
        if (hit.valid) {
            h.distanceSquared = hit.distanceSquared;
            h.triangle = hit.triangleIndex;
            h.baryU = hit.baryU;
            h.baryV = hit.baryV;
            h.point = hit.point;
        }
        batch.hits.push_back(h);
    }
    return batch;
}

MeshClosestHit MeshClosestPoint::closest(const Mesh& mesh, const Vec3& query) {
    MeshClosestPoint cp(mesh);
    return cp.closest(query);
}

MeshClosestBatch MeshClosestPoint::query(const Mesh& mesh, const std::vector<Vec3>& queries) {
    MeshClosestPoint cp(mesh);
    return cp.closestBatch(queries);
}

} // namespace nexus::geometry