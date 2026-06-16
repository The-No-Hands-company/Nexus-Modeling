#include <nexus/geometry/MeshFillet.h>

#include <algorithm>
#include <cmath>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

FilletReport MeshFilletOperation::fillet(HalfEdgeMesh&                mesh,
                                          const std::vector<uint32_t>& halfEdgeIndices,
                                          const FilletDesc&           desc) noexcept
{
    FilletReport report;

    if (desc.radius <= 0.f) {
        report.valid = false;
        return report;
    }

    report.valid = true;

    // Process edges in reverse index order so earlier indices stay valid
    // after splits renumber the edge array.
    std::vector<uint32_t> sortedEdges = halfEdgeIndices;
    std::sort(sortedEdges.begin(), sortedEdges.end(), std::greater<uint32_t>());

    for (uint32_t heIdx : sortedEdges) {
        if (heIdx >= mesh.edgeCount()) continue;

        const auto& he = mesh.edge(heIdx);
        uint32_t twin = he.twin;
        if (twin == HalfEdgeMesh::kInvalid) continue;

        const auto& heTwin = mesh.edge(twin);

        // Must be an interior edge with two valid faces.
        uint32_t f0 = he.face;
        uint32_t f1 = heTwin.face;
        if (f0 == HalfEdgeMesh::kInvalid || f1 == HalfEdgeMesh::kInvalid) continue;

        // Compute the bisector normal from the two face normals.
        auto faceNormal = [&](uint32_t face) -> Vec3 {
            uint32_t fe = mesh.face(face).edge;
            if (fe == HalfEdgeMesh::kInvalid) return {0, 0, 1};
            uint32_t e1 = mesh.edge(fe).next;
            if (e1 == HalfEdgeMesh::kInvalid) return {0, 0, 1};
            uint32_t e2 = mesh.edge(e1).next;
            if (e2 == HalfEdgeMesh::kInvalid) return {0, 0, 1};
            Vec3 a = mesh.positions()[mesh.edge(fe).src];
            Vec3 b = mesh.positions()[mesh.edge(e1).src];
            Vec3 c = mesh.positions()[mesh.edge(e2).src];
            Vec3 n = (b - a).cross(c - a);
            float len = n.length();
            return (len > 1e-10f) ? n * (1.f / len) : Vec3{0, 0, 1};
        };

        Vec3 n0 = faceNormal(f0);
        Vec3 n1 = faceNormal(f1);
        Vec3 bisector = n0 + n1;
        float bLen = bisector.length();
        if (bLen < 1e-10f) continue;
        bisector = bisector * (1.f / bLen);

        // Determine convex/concave: if the edge vector dotted with the bisector
        // is positive, the bisector points toward the edge's convex side.
        // Flip for concave edges so the fillet always curves outward.
        Vec3 pa = mesh.positions()[he.src];
        Vec3 pb = mesh.positions()[heTwin.src];
        Vec3 edgeDir = pb - pa;
        float edgeLen = edgeDir.length();
        if (edgeLen > 1e-10f) {
            edgeDir = edgeDir * (1.f / edgeLen);
        }
        // The face normals point outward (by convention).  The bisector
        // of two face normals points outward at convex edges and inward
        // at concave edges.  Check with the offset vertex-to-face test.
        Vec3 midpoint = (pa + pb) * 0.5f;
        // Project the midpoint + bisector*radius onto each face plane.
        // If it's on the positive side (same as normal), the edge is convex.
        Vec3 offset = midpoint + bisector * desc.radius;
        float d0 = (offset - pa).dot(n0);
        float d1 = (offset - pb).dot(n1);
        if (d0 < 0.f || d1 < 0.f) {
            bisector = bisector * -1.f;  // flip for concave edges
        }

        // Split the edge — creates one new vertex at the midpoint.
        uint32_t oldVc = mesh.vertexCount();
        if (!mesh.splitEdge(heIdx)) continue;

        if (oldVc >= mesh.vertexCount()) continue;
        uint32_t newV = oldVc;

        // Offset the new vertex along the bisector.
        Vec3 filletPos = midpoint + bisector * desc.radius;
        mesh.positions()[newV] = filletPos;

        report.verticesAdded++;
        report.edgesProcessed++;
    }

    return report;
}

FilletReport MeshFilletOperation::filletVariable(
    HalfEdgeMesh& mesh,
    const std::vector<uint32_t>& halfEdgeIndices,
    const std::unordered_map<uint32_t, float>& radiiMap,
    const FilletDesc& desc) noexcept
{
    FilletReport report;
    if (!desc.variableRadius) {
        return fillet(mesh, halfEdgeIndices, desc);
    }
    report.valid = true;

    std::vector<uint32_t> sortedEdges = halfEdgeIndices;
    std::sort(sortedEdges.begin(), sortedEdges.end(), std::greater<uint32_t>());

    for (uint32_t heIdx : sortedEdges) {
        if (heIdx >= mesh.edgeCount()) continue;
        const auto& he = mesh.edge(heIdx);
        uint32_t twin = he.twin;
        if (twin == HalfEdgeMesh::kInvalid) continue;
        const auto& heTwin = mesh.edge(twin);

        uint32_t f0 = he.face, f1 = heTwin.face;
        if (f0 == HalfEdgeMesh::kInvalid || f1 == HalfEdgeMesh::kInvalid) continue;

        // Get the per-edge radius, falling back to desc.radius.
        float edgeRadius = desc.radius;
        auto it = radiiMap.find(heIdx);
        if (it != radiiMap.end()) edgeRadius = it->second;
        else {
            it = radiiMap.find(twin);
            if (it != radiiMap.end()) edgeRadius = it->second;
        }
        if (edgeRadius <= 0.f) continue;

        // Compute bisector and split (same logic as constant fillet).
        auto faceNormal = [&](uint32_t face) -> Vec3 {
            uint32_t fe = mesh.face(face).edge;
            if (fe == HalfEdgeMesh::kInvalid) return {0,0,1};
            uint32_t e1 = mesh.edge(fe).next;
            if (e1 == HalfEdgeMesh::kInvalid) return {0,0,1};
            uint32_t e2 = mesh.edge(e1).next;
            if (e2 == HalfEdgeMesh::kInvalid) return {0,0,1};
            Vec3 a = mesh.positions()[mesh.edge(fe).src];
            Vec3 b = mesh.positions()[mesh.edge(e1).src];
            Vec3 c = mesh.positions()[mesh.edge(e2).src];
            Vec3 n = (b-a).cross(c-a);
            float len = n.length();
            return (len>1e-10f) ? n*(1.f/len) : Vec3{0,0,1};
        };

        Vec3 n0 = faceNormal(f0), n1 = faceNormal(f1);
        Vec3 bisector = n0 + n1;
        float bLen = bisector.length();
        if (bLen < 1e-10f) continue;
        bisector = bisector * (1.f/bLen);

        Vec3 pa = mesh.positions()[he.src];
        Vec3 pb = mesh.positions()[heTwin.src];
        Vec3 edgeDir = pb - pa;
        float edgeLen = edgeDir.length();
        if (edgeLen > 1e-10f) edgeDir = edgeDir * (1.f/edgeLen);

        Vec3 midpoint = (pa+pb)*0.5f;
        Vec3 offset = midpoint + bisector * edgeRadius;
        float d0 = (offset-pa).dot(n0), d1 = (offset-pb).dot(n1);
        if (d0 < 0.f || d1 < 0.f) bisector = bisector * -1.f;

        uint32_t oldVc = mesh.vertexCount();
        if (!mesh.splitEdge(heIdx)) continue;
        if (oldVc >= mesh.vertexCount()) continue;
        uint32_t newV = oldVc;

        mesh.positions()[newV] = midpoint + bisector * edgeRadius;
        report.verticesAdded++;
        report.edgesProcessed++;
    }
    return report;
}

} // namespace nexus::geometry
