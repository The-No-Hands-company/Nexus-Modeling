#include <nexus/geometry/DirectModeling.h>

#include <algorithm>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

DirectModelingReport DirectModeling::pushFaces(
    HalfEdgeMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float distance) noexcept
{
    DirectModelingReport report;
    if (faceIndices.empty() || std::abs(distance) < 1e-10f) return report;

    // Collect all vertices incident to the selected faces.
    std::unordered_set<uint32_t> vertSet;
    for (uint32_t fi : faceIndices) {
        if (fi >= mesh.faceCount()) continue;
        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;
        uint32_t startHe = he;
        do {
            vertSet.insert(mesh.edge(he).src);
            he = mesh.edge(he).next;
        } while (he != startHe);
    }

    // Compute average normal of selected faces and offset each vertex.
    Vec3 avgNormal{0, 0, 0};
    for (uint32_t fi : faceIndices) {
        if (fi >= mesh.faceCount()) continue;
        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;
        Vec3 a = mesh.positions()[mesh.edge(he).src];
        Vec3 b = mesh.positions()[mesh.edge(mesh.edge(he).next).src];
        Vec3 c = mesh.positions()[mesh.edge(mesh.edge(mesh.edge(he).next).next).src];
        Vec3 n = (b - a).cross(c - a);
        float len = n.length();
        if (len > 1e-10f) avgNormal = avgNormal + n * (1.f / len);
    }
    float normLen = avgNormal.length();
    if (normLen > 1e-10f) avgNormal = avgNormal * (1.f / normLen);

    // Offset each vertex along the average normal.
    for (uint32_t v : vertSet) {
        if (v < mesh.vertexCount())
            mesh.positions()[v] = mesh.positions()[v] + avgNormal * distance;
    }

    report.valid = true;
    report.facesMoved = static_cast<uint32_t>(faceIndices.size());
    report.verticesAdded = 0;
    return report;
}

DirectModelingReport DirectModeling::pullFaces(
    HalfEdgeMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float distance) noexcept
{
    return pushFaces(mesh, faceIndices, -distance);
}

} // namespace nexus::geometry
