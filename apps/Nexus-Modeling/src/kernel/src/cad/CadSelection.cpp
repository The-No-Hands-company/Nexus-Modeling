#include <nexus/cad/CadSelection.h>
#include <nexus/geometry/MeshBVH.h>

#include <limits>

namespace nexus::cad {

void CadSelection::clear()
{
    m_items.clear();
}

void CadSelection::addFace(uint32_t faceIndex)
{
    m_items.push_back({SelectionKind::Face, faceIndex, "Face " + std::to_string(faceIndex)});
}

void CadSelection::addEdge(uint32_t edgeIndex)
{
    m_items.push_back({SelectionKind::Edge, edgeIndex, "Edge " + std::to_string(edgeIndex)});
}

void CadSelection::addVertex(uint32_t vertexIndex)
{
    m_items.push_back({SelectionKind::Vertex, vertexIndex, "Vertex " + std::to_string(vertexIndex)});
}

void CadSelection::addSketchEntity(uint32_t entityId)
{
    m_items.push_back({SelectionKind::SketchEntity, entityId, "Entity " + std::to_string(entityId)});
}

int32_t CadSelection::pickFace(const geometry::Mesh& mesh,
                                 const Vec3& rayOrigin,
                                 const Vec3& rayDirection) const noexcept
{
    geometry::MeshBVH bvh;
    bvh.build(mesh);
    if (!bvh.isValid()) return -1;

    geometry::Ray ray;
    ray.origin = rayOrigin;
    ray.direction = rayDirection;

    geometry::RayHit hit = bvh.raycast(ray);
    if (hit.t < std::numeric_limits<float>::max())
        return static_cast<int32_t>(hit.triangleIndex);
    return -1;
}

std::vector<uint32_t> CadSelection::pickFacesInRect(
    const geometry::Mesh& mesh,
    const Vec3& rectMin,
    const Vec3& rectMax) const noexcept
{
    (void)mesh; (void)rectMin; (void)rectMax;
    // Simplified: return empty for MVP.
    return {};
}

} // namespace nexus::cad
