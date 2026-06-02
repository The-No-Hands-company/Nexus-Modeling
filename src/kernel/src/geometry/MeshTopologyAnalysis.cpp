#include <nexus/geometry/MeshTopologyAnalysis.h>

#include <vector>

namespace nexus::geometry {

MeshTopologyAnalysis MeshTopologyAnalyser::analyse(const HalfEdgeMesh& mesh) noexcept
{
    MeshTopologyAnalysis result;

    result.vertexCount     = mesh.vertexCount();
    result.faceCount       = mesh.faceCount();
    result.isManifold      = mesh.isManifold();
    result.isClosed        = mesh.isClosed();
    result.isTriangulated  = mesh.isTriangulated();

    // ── Edge count ────────────────────────────────────────────────────────────
    // Each undirected edge is represented by two directed half-edges when interior,
    // or by one directed half-edge when on the boundary.
    // Count: interior edges contribute 2 half-edges, boundary edges contribute 1.
    uint32_t interiorHalfEdges = 0;
    uint32_t boundaryHalfEdges = 0;
    for (const auto& he : mesh.halfEdges()) {
        if (he.twin != kHEInvalid) {
            ++interiorHalfEdges;
        } else {
            ++boundaryHalfEdges;
        }
    }
    // Each interior undirected edge is counted twice.
    const uint32_t edgeCount = (interiorHalfEdges / 2) + boundaryHalfEdges;
    result.edgeCount = edgeCount;

    // ── Euler characteristic ──────────────────────────────────────────────────
    result.eulerCharacteristic =
        static_cast<int32_t>(result.vertexCount) -
        static_cast<int32_t>(edgeCount) +
        static_cast<int32_t>(result.faceCount);

    // ── Boundary loops ────────────────────────────────────────────────────────
    const auto loops = mesh.boundaryLoops();
    result.boundaryLoopCount = static_cast<uint32_t>(loops.size());

    // ── Connected components — BFS over vertex adjacency via half-edges ───────
    {
        const uint32_t nv = mesh.vertexCount();
        std::vector<bool> visited(nv, false);
        uint32_t components = 0;

        for (uint32_t startV = 0; startV < nv; ++startV) {
            if (visited[startV]) {
                continue;
            }
            // BFS from startV through the half-edge graph.
            std::vector<uint32_t> queue;
            queue.push_back(startV);
            visited[startV] = true;
            size_t head = 0;

            while (head < queue.size()) {
                const uint32_t v = queue[head++];
                const uint32_t outgoing = mesh.vertices()[v].outgoing;
                if (outgoing == kHEInvalid) {
                    continue;
                }
                // Walk the fan of outgoing half-edges.
                uint32_t cur = outgoing;
                uint32_t guard = 0;
                do {
                    const uint32_t dst = mesh.halfEdges()[cur].dst;
                    if (dst < nv && !visited[dst]) {
                        visited[dst] = true;
                        queue.push_back(dst);
                    }
                    const uint32_t twin = mesh.halfEdges()[cur].twin;
                    if (twin == kHEInvalid) {
                        break;
                    }
                    cur = mesh.halfEdges()[twin].next;
                    if (++guard > mesh.halfEdgeCount()) {
                        break;
                    }
                } while (cur != outgoing);
            }
            ++components;
        }
        result.connectedComponents = components;
    }

    // ── Genus ─────────────────────────────────────────────────────────────────
    // For a closed, connected, orientable surface: genus = (2 − χ) / 2.
    // For an open surface: use the generalised formula with boundary loops.
    // For non-manifold or multi-component meshes: set genus = -1 (undefined).
    if (result.isManifold && result.connectedComponents == 1) {
        if (result.isClosed) {
            const int32_t chi = result.eulerCharacteristic;
            result.genus = (2 - chi) / 2;
        } else {
            // χ = 2 − 2g − b  →  g = (2 − χ − b) / 2
            const int32_t chi = result.eulerCharacteristic;
            const int32_t b   = static_cast<int32_t>(result.boundaryLoopCount);
            const int32_t numerator = 2 - chi - b;
            if (numerator >= 0 && numerator % 2 == 0) {
                result.genus = numerator / 2;
            }
        }
    }

    return result;
}

} // namespace nexus::geometry
