#include <nexus/geometry/BRepBody.h>

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace nexus::geometry {

// ── Construction ─────────────────────────────────────────────────────────────

std::pair<BRepBody, BRepReport> BRepBody::fromMesh(const Mesh& mesh) noexcept
{
    BRepBody body;
    BRepReport report;

    std::optional<HalfEdgeMesh> heOpt = HalfEdgeMesh::fromMesh(mesh);
    if (!heOpt) {
        report.valid = false;
        report.errors.push_back("failed to build half-edge mesh from input mesh");
        return {body, report};
    }

    body.m_heMesh = std::move(*heOpt);

    // Non-manifold meshes are rejected; open (boundary) meshes are accepted
    // but will produce open shells rather than closed solids.

    const uint32_t vc = body.m_heMesh.vertexCount();
    const uint32_t ec = body.m_heMesh.edgeCount();
    const uint32_t fc = body.m_heMesh.faceCount();

    // Build vertices.
    body.m_vertices.reserve(vc);
    for (uint32_t i = 0; i < vc; ++i) {
        BRepVertex v;
        v.id       = i;
        v.point    = body.m_heMesh.positions()[i];
        v.halfEdge = body.m_heMesh.vertex(i).edge;
        body.m_vertices.push_back(v);
    }

    // Build edges (one per twin pair).
    // Track which half-edges have been paired to avoid duplicates.
    std::vector<bool> edgeVisited(ec, false);
    body.m_edges.reserve(ec / 2);

    for (uint32_t he = 0; he < ec; ++he) {
        if (edgeVisited[he]) continue;

        const auto& e = body.m_heMesh.edge(he);
        uint32_t twin = e.twin;

        BRepEdge be;
        be.id       = static_cast<BRepEdgeId>(body.m_edges.size());
        be.halfEdge = he;
        be.v0       = e.src;
        be.v1       = (twin != HalfEdgeMesh::kInvalid) ? body.m_heMesh.edge(twin).src : HalfEdgeMesh::kInvalid;
        body.m_edges.push_back(be);

        body.m_halfEdgeToEdge[he] = be.id;
        if (twin != HalfEdgeMesh::kInvalid) {
            body.m_halfEdgeToEdge[twin] = be.id;
        }

        edgeVisited[he] = true;
        if (twin != HalfEdgeMesh::kInvalid) {
            edgeVisited[twin] = true;
        }
    }

    // Build faces.
    body.m_faces.reserve(fc);
    for (uint32_t i = 0; i < fc; ++i) {
        BRepFace f;
        f.id       = i;
        f.halfEdge = body.m_heMesh.face(i).edge;
        f.shell    = kInvalidBRepShell;
        body.m_faces.push_back(f);
    }

    // Build shells (connected face components).
    std::vector<bool> faceVisited(fc, false);

    for (uint32_t startFace = 0; startFace < fc; ++startFace) {
        if (faceVisited[startFace]) continue;

        BRepShell shell;
        shell.id = static_cast<BRepShellId>(body.m_shells.size());

        std::queue<uint32_t> queue;
        queue.push(startFace);
        faceVisited[startFace] = true;

        while (!queue.empty()) {
            uint32_t fi = queue.front();
            queue.pop();

            body.m_faces[fi].shell = shell.id;
            shell.faces.push_back(fi);

            // Walk the face's half-edges to find adjacent faces via twins.
            uint32_t startHe = body.m_heMesh.face(fi).edge;
            if (startHe == HalfEdgeMesh::kInvalid) continue;

            uint32_t he = startHe;
            do {
                const auto& e = body.m_heMesh.edge(he);
                if (e.twin != HalfEdgeMesh::kInvalid) {
                    uint32_t neighborFace = body.m_heMesh.edge(e.twin).face;
                    if (neighborFace != HalfEdgeMesh::kInvalid && !faceVisited[neighborFace]) {
                        faceVisited[neighborFace] = true;
                        queue.push(neighborFace);
                    }
                }
                he = e.next;
            } while (he != startHe);
        }

        // Check if the shell is closed (no boundary edges).
        shell.closed = true;
        for (BRepFaceId fi : shell.faces) {
            uint32_t startHe = body.m_heMesh.face(fi).edge;
            if (startHe == HalfEdgeMesh::kInvalid) { shell.closed = false; break; }
            uint32_t he = startHe;
            do {
                if (body.m_heMesh.edge(he).twin == HalfEdgeMesh::kInvalid) {
                    shell.closed = false;
                    break;
                }
                he = body.m_heMesh.edge(he).next;
            } while (he != startHe);
            if (!shell.closed) break;
        }

        body.m_shells.push_back(std::move(shell));
    }

    report.valid = true;
    return {std::move(body), report};
}

// ── Export ───────────────────────────────────────────────────────────────────

Mesh BRepBody::toMesh() const
{
    return m_heMesh.toMesh();
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool BRepBody::isManifold() const noexcept
{
    return m_heMesh.isManifold();
}

bool BRepBody::isClosed() const noexcept
{
    return m_heMesh.isClosed();
}

int32_t BRepBody::eulerCharacteristic() const noexcept
{
    // χ = V - E + F
    return static_cast<int32_t>(vertexCount())
         - static_cast<int32_t>(edgeCount())
         + static_cast<int32_t>(faceCount());
}

int32_t BRepBody::genus() const noexcept
{
    if (!isClosed()) return -1;
    int32_t chi = eulerCharacteristic();
    return (2 - chi) / 2;
}

// ── Entity access ────────────────────────────────────────────────────────────

const BRepVertex& BRepBody::vertex(BRepVertexId id) const noexcept
{
    return m_vertices[id];
}

const BRepEdge& BRepBody::edge(BRepEdgeId id) const noexcept
{
    return m_edges[id];
}

const BRepFace& BRepBody::face(BRepFaceId id) const noexcept
{
    return m_faces[id];
}

const BRepShell& BRepBody::shell(BRepShellId id) const noexcept
{
    return m_shells[id];
}

// ── Adjacency ────────────────────────────────────────────────────────────────

std::vector<BRepFaceId> BRepBody::vertexFaces(BRepVertexId v) const noexcept
{
    if (v >= m_vertices.size()) return {};
    auto heFaces = m_heMesh.vertexFaces(v);
    std::vector<BRepFaceId> result;
    result.reserve(heFaces.size());
    for (auto f : heFaces) result.push_back(static_cast<BRepFaceId>(f));
    return result;
}

std::vector<BRepFaceId> BRepBody::edgeFaces(BRepEdgeId e) const noexcept
{
    if (e >= m_edges.size()) return {};
    std::vector<BRepFaceId> result;

    const auto& be = m_edges[e];
    if (be.halfEdge != HalfEdgeMesh::kInvalid) {
        uint32_t f = m_heMesh.edge(be.halfEdge).face;
        if (f != HalfEdgeMesh::kInvalid)
            result.push_back(static_cast<BRepFaceId>(f));
    }

    uint32_t twin = (be.halfEdge != HalfEdgeMesh::kInvalid)
        ? m_heMesh.edge(be.halfEdge).twin : HalfEdgeMesh::kInvalid;
    if (twin != HalfEdgeMesh::kInvalid) {
        uint32_t f = m_heMesh.edge(twin).face;
        if (f != HalfEdgeMesh::kInvalid)
            result.push_back(static_cast<BRepFaceId>(f));
    }

    return result;
}

std::vector<BRepEdgeId> BRepBody::faceEdges(BRepFaceId f) const noexcept
{
    if (f >= m_faces.size()) return {};

    std::vector<BRepEdgeId> result;
    uint32_t startHe = m_faces[f].halfEdge;
    if (startHe == HalfEdgeMesh::kInvalid) return result;

    uint32_t he = startHe;
    do {
        const auto& e = m_heMesh.edge(he);

        auto it = m_halfEdgeToEdge.find(he);
        if (it != m_halfEdgeToEdge.end() &&
            std::find(result.begin(), result.end(), it->second) == result.end())
            result.push_back(it->second);

        he = e.next;
    } while (he != startHe);

    return result;
}

std::vector<BRepVertexId> BRepBody::faceVertices(BRepFaceId f) const noexcept
{
    if (f >= m_faces.size()) return {};

    std::vector<BRepVertexId> result;
    uint32_t startHe = m_faces[f].halfEdge;
    if (startHe == HalfEdgeMesh::kInvalid) return result;

    uint32_t he = startHe;
    do {
        const auto& e = m_heMesh.edge(he);
        result.push_back(static_cast<BRepVertexId>(e.src));
        he = e.next;
    } while (he != startHe);

    return result;
}

} // namespace nexus::geometry
