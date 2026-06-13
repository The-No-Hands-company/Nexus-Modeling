#include <nexus/geometry/MeshTopologyAnalysis.h>
#include <nexus/geometry/HalfEdgeMesh.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <queue>
#include <vector>

namespace nexus::geometry {

TopologyInfo MeshTopologyAnalyser::analyse(const Mesh& mesh) {
    TopologyInfo info{};

    info.vertices = static_cast<uint32_t>(mesh.attributes().vertexCount());
    info.faces = static_cast<uint32_t>(mesh.topology().faceCount());

    for (size_t i = 0; i < mesh.topology().faceCount(); ++i) {
        if (!mesh.topology().face(i).isTriangle()) {
            info.isTriangulated = false;
            break;
        }
    }

    auto hemOpt = HalfEdgeMesh::fromMesh(mesh);
    if (!hemOpt.has_value()) {
        info.isTriangulated = (info.faces > 0) ? info.isTriangulated : true;
        return info;
    }
    auto& hem = *hemOpt;

    info.edges = hem.edgeCount() / 2;
    info.boundaryLoops = static_cast<uint32_t>(hem.boundaryLoops().size());

    info.euler = static_cast<int>(info.vertices) - static_cast<int>(info.edges) + static_cast<int>(info.faces);

    // Genus for orientable surfaces: g = (2 - euler - boundaryLoops) / 2
    info.genus = (2 - info.euler - static_cast<int>(info.boundaryLoops)) / 2;
    if (info.genus < 0) info.genus = 0;

    info.isManifold = hem.isManifold();
    info.isClosed = hem.isClosed();
    info.isTriangulated = hem.isTriangulated() && info.isTriangulated;

    // Connected components via face-adjacency BFS
    uint32_t faceCount = hem.faceCount();
    if (faceCount > 0) {
        std::vector<bool> visited(faceCount, false);
        for (uint32_t f = 0; f < faceCount; ++f) {
            if (visited[f]) continue;
            ++info.components;
            std::queue<uint32_t> q;
            q.push(f);
            visited[f] = true;
            while (!q.empty()) {
                uint32_t cur = q.front(); q.pop();
                uint32_t startEdge = hem.face(cur).edge;
                if (startEdge == HalfEdgeMesh::kInvalid) continue;
                uint32_t e = startEdge;
                do {
                    uint32_t twin = hem.edge(e).twin;
                    if (twin != HalfEdgeMesh::kInvalid) {
                        uint32_t adjFace = hem.edge(twin).face;
                        if (adjFace != HalfEdgeMesh::kInvalid && !visited[adjFace]) {
                            visited[adjFace] = true;
                            q.push(adjFace);
                        }
                    }
                    e = hem.edge(e).next;
                    if (e == HalfEdgeMesh::kInvalid) break;
                } while (e != startEdge);
            }
        }
    }

    return info;
}

} // namespace nexus::geometry
