#include <nexus/geometry/HalfEdgeMesh.h>

#include <algorithm>
#include <map>
#include <unordered_map>
#include <utility>

namespace nexus::geometry {

namespace {

// Canonical directed-edge key: (src, dst) — lower src first only for the twin lookup map.
struct EdgeKey {
    uint32_t src;
    uint32_t dst;
    bool operator<(const EdgeKey& o) const noexcept
    {
        return src < o.src || (src == o.src && dst < o.dst);
    }
    bool operator==(const EdgeKey& o) const noexcept { return src == o.src && dst == o.dst; }
};

} // namespace

// ─── Construction ─────────────────────────────────────────────────────────────

std::optional<HalfEdgeMesh> HalfEdgeMesh::fromMesh(const Mesh& mesh)
{
    const MeshTopology& topo = mesh.topology();
    const MeshAttributes& attr = mesh.attributes();

    const uint32_t numVerts = static_cast<uint32_t>(attr.vertexCount());
    const uint32_t numFaces = static_cast<uint32_t>(topo.faceCount());

    if (numVerts == 0 || numFaces == 0) {
        return std::nullopt;
    }

    HalfEdgeMesh hem;

    // ── Copy vertex positions ─────────────────────────────────────────────────
    hem.m_vertices.resize(numVerts);
    const auto& positions = attr.positions();
    for (uint32_t v = 0; v < numVerts; ++v) {
        hem.m_vertices[v].position = positions[v];
    }

    // ── Allocate half-edges and faces ─────────────────────────────────────────
    // Count half-edges first.
    uint32_t totalHE = 0;
    for (uint32_t f = 0; f < numFaces; ++f) {
        const Face& face = topo.face(f);
        if (face.indices.size() < 3) {
            return std::nullopt; // degenerate face
        }
        totalHE += static_cast<uint32_t>(face.indices.size());
    }

    hem.m_halfEdges.resize(totalHE);
    hem.m_faces.resize(numFaces);

    // ── Build per-face half-edges and fill next/prev/src/dst/face ────────────
    // edge_map: directed (src, dst) → half-edge index
    std::map<EdgeKey, uint32_t> edgeMap;

    uint32_t heBase = 0;
    for (uint32_t f = 0; f < numFaces; ++f) {
        const Face& face = topo.face(f);
        const uint32_t nv = static_cast<uint32_t>(face.indices.size());

        hem.m_faces[f].halfEdge = heBase;

        for (uint32_t k = 0; k < nv; ++k) {
            const uint32_t heIdx  = heBase + k;
            const uint32_t nextK  = (k + 1) % nv;
            const uint32_t prevK  = (k + nv - 1) % nv;

            HalfEdgeRecord& he = hem.m_halfEdges[heIdx];
            he.src  = face.indices[k];
            he.dst  = face.indices[nextK];
            he.face = f;
            he.next = heBase + nextK;
            he.prev = heBase + prevK;
            he.twin = kHEInvalid; // filled below

            // Non-manifold check: same directed edge used twice.
            const EdgeKey key{he.src, he.dst};
            if (edgeMap.count(key)) {
                return std::nullopt;
            }
            edgeMap[key] = heIdx;
        }

        heBase += nv;
    }

    // ── Link twins ────────────────────────────────────────────────────────────
    for (auto& [key, heIdx] : edgeMap) {
        const EdgeKey twinKey{key.dst, key.src};
        const auto it = edgeMap.find(twinKey);
        if (it != edgeMap.end()) {
            hem.m_halfEdges[heIdx].twin          = it->second;
            hem.m_halfEdges[it->second].twin     = heIdx;
        }
        // Boundary edges keep twin = kHEInvalid.
    }

    // ── Set vertex outgoing half-edges ────────────────────────────────────────
    for (uint32_t h = 0; h < totalHE; ++h) {
        const uint32_t v = hem.m_halfEdges[h].src;
        if (v < numVerts && hem.m_vertices[v].outgoing == kHEInvalid) {
            hem.m_vertices[v].outgoing = h;
        }
    }

    return hem;
}

// ─── Export ───────────────────────────────────────────────────────────────────

Mesh HalfEdgeMesh::toMesh() const
{
    Mesh out;
    MeshAttributes attrs;
    std::vector<nexus::render::Vec3> positions;
    positions.reserve(m_vertices.size());
    for (const auto& v : m_vertices) {
        positions.push_back(v.position);
    }
    attrs.setPositions(std::move(positions));
    out.attributes() = std::move(attrs);

    for (uint32_t f = 0; f < static_cast<uint32_t>(m_faces.size()); ++f) {
        const auto verts = faceVertices(f);
        if (verts.size() == 3) {
            Face face;
            face.indices = {verts[0], verts[1], verts[2]};
            out.topology().addFace(std::move(face));
        } else if (verts.size() >= 3) {
            // Fan triangulate
            for (uint32_t i = 1; i + 1 < static_cast<uint32_t>(verts.size()); ++i) {
                Face face;
                face.indices = {verts[0], verts[i], verts[i + 1]};
                out.topology().addFace(std::move(face));
            }
        }
    }

    return out;
}

// ─── Topology predicates ──────────────────────────────────────────────────────

bool HalfEdgeMesh::isClosed() const noexcept
{
    for (const auto& he : m_halfEdges) {
        if (he.twin == kHEInvalid) {
            return false;
        }
    }
    return true;
}

bool HalfEdgeMesh::isManifold() const noexcept
{
    // Each directed edge appears at most once (guaranteed by fromMesh), and
    // each undirected edge has at most two incident faces.  Additionally, the
    // vertex fan around every vertex must be connected.
    for (const auto& he : m_halfEdges) {
        if (he.src == kHEInvalid || he.dst == kHEInvalid) {
            return false;
        }
    }
    // Verify vertex fans are connected (single loop around each interior vertex).
    for (uint32_t v = 0; v < static_cast<uint32_t>(m_vertices.size()); ++v) {
        uint32_t start = m_vertices[v].outgoing;
        if (start == kHEInvalid) {
            continue;
        }
        // Walk the outgoing fan via twin->next links.
        uint32_t cur = start;
        uint32_t steps = 0;
        do {
            const uint32_t twin = m_halfEdges[cur].twin;
            if (twin == kHEInvalid) {
                break; // boundary vertex — fan is open, that's allowed
            }
            cur = m_halfEdges[twin].next;
            ++steps;
            if (steps > static_cast<uint32_t>(m_halfEdges.size())) {
                return false; // infinite loop → non-manifold
            }
        } while (cur != start);
    }
    return true;
}

bool HalfEdgeMesh::isTriangulated() const noexcept
{
    for (uint32_t f = 0; f < static_cast<uint32_t>(m_faces.size()); ++f) {
        if (faceVertices(f).size() != 3) {
            return false;
        }
    }
    return true;
}

// ─── Adjacency traversal ──────────────────────────────────────────────────────

std::vector<uint32_t> HalfEdgeMesh::faceVertices(uint32_t face) const
{
    std::vector<uint32_t> result;
    if (face >= m_faces.size()) {
        return result;
    }
    uint32_t start = m_faces[face].halfEdge;
    if (start == kHEInvalid) {
        return result;
    }
    uint32_t cur = start;
    do {
        result.push_back(m_halfEdges[cur].src);
        cur = m_halfEdges[cur].next;
        if (result.size() > m_halfEdges.size()) {
            break; // guard against corrupt mesh
        }
    } while (cur != start);
    return result;
}

std::vector<uint32_t> HalfEdgeMesh::vertexFaces(uint32_t vertex) const
{
    std::vector<uint32_t> result;
    if (vertex >= m_vertices.size()) {
        return result;
    }
    const uint32_t start = m_vertices[vertex].outgoing;
    if (start == kHEInvalid) {
        return result;
    }
    uint32_t cur = start;
    do {
        if (m_halfEdges[cur].face != kHEInvalid) {
            result.push_back(m_halfEdges[cur].face);
        }
        const uint32_t twin = m_halfEdges[cur].twin;
        if (twin == kHEInvalid) {
            break; // open boundary — can't complete the loop
        }
        cur = m_halfEdges[twin].next;
        if (result.size() > m_halfEdges.size()) {
            break;
        }
    } while (cur != start);
    return result;
}

std::vector<std::vector<uint32_t>> HalfEdgeMesh::boundaryLoops() const
{
    std::vector<std::vector<uint32_t>> loops;
    std::vector<bool> visited(m_halfEdges.size(), false);

    for (uint32_t h = 0; h < static_cast<uint32_t>(m_halfEdges.size()); ++h) {
        if (m_halfEdges[h].twin != kHEInvalid || visited[h]) {
            continue;
        }
        // Start of a boundary edge.  Walk the boundary loop by following the
        // "next boundary half-edge" via: from a boundary HE, go next inside its
        // face polygon until we find another boundary HE leaving that face.
        std::vector<uint32_t> loop;
        uint32_t cur = h;
        do {
            visited[cur] = true;
            loop.push_back(m_halfEdges[cur].src);
            // Advance: walk the face loop from cur's twin's face until we find
            // the next boundary edge whose src is cur's dst.
            uint32_t candidate = m_halfEdges[cur].next;
            uint32_t guard = 0;
            while (m_halfEdges[candidate].twin != kHEInvalid) {
                candidate = m_halfEdges[m_halfEdges[candidate].twin].next;
                if (++guard > m_halfEdges.size()) {
                    break;
                }
            }
            cur = candidate;
        } while (cur != h && loop.size() <= m_halfEdges.size());

        if (!loop.empty()) {
            loops.push_back(std::move(loop));
        }
    }
    return loops;
}

// ─── Edge operations ──────────────────────────────────────────────────────────

bool HalfEdgeMesh::flipEdge(uint32_t he)
{
    if (he >= m_halfEdges.size()) {
        return false;
    }
    const uint32_t twin = m_halfEdges[he].twin;
    if (twin == kHEInvalid) {
        return false; // boundary edge — cannot flip
    }

    // Both faces must be triangles.
    if (faceVertices(m_halfEdges[he].face).size() != 3 ||
        faceVertices(m_halfEdges[twin].face).size() != 3) {
        return false;
    }

    // Gather the 4 half-edges around the two triangles.
    // After flip the shared edge becomes v2-v3 (the two opposite apices).

    const uint32_t heNext = m_halfEdges[he].next;
    const uint32_t hePrev = m_halfEdges[he].prev;
    const uint32_t twNext = m_halfEdges[twin].next;
    const uint32_t twPrev = m_halfEdges[twin].prev;

    const uint32_t v0 = m_halfEdges[he].src;   // he: v0→v1
    const uint32_t v1 = m_halfEdges[he].dst;
    const uint32_t v2 = m_halfEdges[heNext].dst; // apex of he's triangle
    const uint32_t v3 = m_halfEdges[twNext].dst; // apex of twin's triangle

    const uint32_t fA = m_halfEdges[he].face;
    const uint32_t fB = m_halfEdges[twin].face;

    // Rewire: he becomes v2→v3, twin becomes v3→v2.
    m_halfEdges[he].src  = v2;
    m_halfEdges[he].dst  = v3;
    m_halfEdges[twin].src = v3;
    m_halfEdges[twin].dst = v2;

    // Triangle fA: v2→v3 (he), v3→v0 (twPrev), v0→v2 (hePrev)
    m_halfEdges[he].next    = twPrev;
    m_halfEdges[he].prev    = hePrev;
    m_halfEdges[twPrev].next = hePrev;
    m_halfEdges[twPrev].prev = he;
    m_halfEdges[hePrev].next = he;
    m_halfEdges[hePrev].prev = twPrev;

    m_halfEdges[he].face    = fA;
    m_halfEdges[twPrev].face = fA;
    m_halfEdges[hePrev].face = fA;
    m_faces[fA].halfEdge     = he;

    // Triangle fB: v3→v2 (twin), v2→v1 (heNext), v1→v3 (twNext)
    m_halfEdges[twin].next   = heNext;
    m_halfEdges[twin].prev   = twNext;
    m_halfEdges[heNext].next = twNext;
    m_halfEdges[heNext].prev = twin;
    m_halfEdges[twNext].next = twin;
    m_halfEdges[twNext].prev = heNext;

    m_halfEdges[twin].face   = fB;
    m_halfEdges[heNext].face = fB;
    m_halfEdges[twNext].face = fB;
    m_faces[fB].halfEdge     = twin;

    // Fix vertex outgoing pointers for v0 and v1 which lost their original outgoing edge.
    m_vertices[v0].outgoing = hePrev;
    m_vertices[v1].outgoing = twPrev;

    return true;
}

uint32_t HalfEdgeMesh::splitEdge(uint32_t he)
{
    if (he >= m_halfEdges.size()) {
        return kHEInvalid;
    }

    const uint32_t twin = m_halfEdges[he].twin;

    // Gather existing topology.
    const uint32_t heNext = m_halfEdges[he].next;
    const uint32_t fA     = m_halfEdges[he].face;
    const uint32_t vSrc   = m_halfEdges[he].src;
    const uint32_t vDst   = m_halfEdges[he].dst;

    // Midpoint vertex.
    const nexus::render::Vec3& pSrc = m_vertices[vSrc].position;
    const nexus::render::Vec3& pDst = m_vertices[vDst].position;
    nexus::render::Vec3 mid{
        (pSrc.x + pDst.x) * 0.5f,
        (pSrc.y + pDst.y) * 0.5f,
        (pSrc.z + pDst.z) * 0.5f};

    const uint32_t vMid = static_cast<uint32_t>(m_vertices.size());
    m_vertices.push_back(HEVertexRecord{kHEInvalid, mid});

    // ── Interior edge (he) side ───────────────────────────────────────────────
    // New half-edges for the he side: heNew (vMid→vDst) replaces the second half.
    const uint32_t heNew = static_cast<uint32_t>(m_halfEdges.size());
    m_halfEdges.push_back(HalfEdgeRecord{});

    // he is now vSrc→vMid; heNew is vMid→vDst.
    m_halfEdges[he].dst       = vMid;
    m_halfEdges[heNew].src    = vMid;
    m_halfEdges[heNew].dst    = vDst;
    m_halfEdges[heNew].face   = fA;
    m_halfEdges[heNew].next   = heNext;
    m_halfEdges[heNew].prev   = he;
    m_halfEdges[heNew].twin   = kHEInvalid; // set below

    m_halfEdges[he].next      = heNew;
    m_halfEdges[heNext].prev  = heNew;
    m_vertices[vMid].outgoing = heNew;

    // Fix face pointer (may still be he — that's fine).
    m_faces[fA].halfEdge = he;

    // ── Twin side (if interior edge) ──────────────────────────────────────────
    if (twin != kHEInvalid) {
        const uint32_t fB     = m_halfEdges[twin].face;

        // Twin is currently vDst→vSrc; split into:
        //   twNew: vDst→vMid  (new, replaces twin's first half)
        //   twin : vMid→vSrc  (existing, becomes the second half)
        const uint32_t twNew = static_cast<uint32_t>(m_halfEdges.size());
        m_halfEdges.push_back(HalfEdgeRecord{});

        m_halfEdges[twNew].src  = vDst;
        m_halfEdges[twNew].dst  = vMid;
        m_halfEdges[twNew].face = fB;
        m_halfEdges[twNew].prev = m_halfEdges[twin].prev;
        m_halfEdges[twNew].next = twin;
        m_halfEdges[twNew].twin = heNew;

        m_halfEdges[m_halfEdges[twNew].prev].next = twNew;
        m_halfEdges[twin].src  = vMid;
        m_halfEdges[twin].prev = twNew;
        m_halfEdges[twin].twin = he;

        m_halfEdges[he].twin   = twin;
        m_halfEdges[heNew].twin = twNew;

        m_faces[fB].halfEdge = twNew;

        // Fix vDst outgoing if it pointed at the old twin.
        if (m_vertices[vDst].outgoing == twin) {
            m_vertices[vDst].outgoing = twNew;
        }
    }

    return vMid;
}

uint32_t HalfEdgeMesh::collapseEdge(uint32_t he)
{
    if (he >= m_halfEdges.size()) {
        return kHEInvalid;
    }

    const uint32_t twin = m_halfEdges[he].twin;
    const uint32_t vSrc = m_halfEdges[he].src;
    const uint32_t vDst = m_halfEdges[he].dst;

    if (vSrc == kHEInvalid || vDst == kHEInvalid) {
        return kHEInvalid;
    }

    // ── Link condition check ──────────────────────────────────────────────────
    // Collect the one-ring (neighbours) of vSrc and vDst.
    // The intersection must contain exactly the shared neighbours
    // (apex vertices of the one or two incident faces).

    auto oneRing = [&](uint32_t v) -> std::vector<uint32_t> {
        std::vector<uint32_t> ring;
        const uint32_t start = m_vertices[v].outgoing;
        if (start == kHEInvalid) {
            return ring;
        }

        uint32_t cur = start;
        uint32_t guard = 0;
        bool hitBoundaryForward = false;
        do {
            ring.push_back(m_halfEdges[cur].dst);
            const uint32_t tw = m_halfEdges[cur].twin;
            if (tw == kHEInvalid) { hitBoundaryForward = true; break; }
            cur = m_halfEdges[tw].next;
            if (++guard > m_halfEdges.size()) { hitBoundaryForward = true; break; }
        } while (cur != start);

        if (!hitBoundaryForward) {
            return ring; // closed fan — complete
        }

        // Open fan: walk backward from start to collect the other side.
        cur = m_halfEdges[start].prev;
        guard = 0;
        while (true) {
            ring.push_back(m_halfEdges[cur].src);
            const uint32_t tw = m_halfEdges[cur].twin;
            if (tw == kHEInvalid) { break; }
            cur = m_halfEdges[tw].prev;
            if (++guard > m_halfEdges.size()) { break; }
        }
        return ring;
    };

    const auto ringSrc = oneRing(vSrc);
    const auto ringDst = oneRing(vDst);

    // Shared neighbours.
    std::vector<uint32_t> shared;
    for (uint32_t v : ringSrc) {
        if (v == vDst) { continue; }
        for (uint32_t w : ringDst) {
            if (v == w) { shared.push_back(v); break; }
        }
    }

    // Expected number of shared neighbours:
    // interior edge: 2 (the two apex vertices)
    // boundary edge: 1
    const uint32_t expectedShared = (twin != kHEInvalid) ? 2u : 1u;
    if (shared.size() != expectedShared) {
        return kHEInvalid; // link condition violated
    }

    // ── Compute midpoint ──────────────────────────────────────────────────────
    const nexus::render::Vec3& pSrc = m_vertices[vSrc].position;
    const nexus::render::Vec3& pDst = m_vertices[vDst].position;
    const nexus::render::Vec3 mid{
        (pSrc.x + pDst.x) * 0.5f,
        (pSrc.y + pDst.y) * 0.5f,
        (pSrc.z + pDst.z) * 0.5f};

    // ── Rewire: redirect all half-edges pointing to vDst to point to vSrc ───
    // vSrc becomes the survivor at the midpoint position.
    m_vertices[vSrc].position = mid;

    for (auto& heRec : m_halfEdges) {
        if (heRec.src == vDst) { heRec.src = vSrc; }
        if (heRec.dst == vDst) { heRec.dst = vSrc; }
    }

    // ── Remove the two incident faces (he's face and twin's face) ────────────
    // Mark both faces and their half-edges as invalid.

    auto removeHEFace = [&](uint32_t faceHE) {
        if (faceHE == kHEInvalid) { return; }
        // Walk the face loop and mark all half-edges invalid.
        uint32_t cur = faceHE;
        uint32_t guard = 0;
        do {
            const uint32_t nextHE = m_halfEdges[cur].next;
            const uint32_t curTwin = m_halfEdges[cur].twin;
            if (curTwin != kHEInvalid) {
                m_halfEdges[curTwin].twin = kHEInvalid;
            }
            m_halfEdges[cur].face = kHEInvalid;
            cur = nextHE;
            if (++guard > m_halfEdges.size()) { break; }
        } while (cur != faceHE);
    };

    // Capture face indices BEFORE removeHEFace clears them.
    const uint32_t faceA = m_halfEdges[he].face;
    const uint32_t faceB = (twin != kHEInvalid) ? m_halfEdges[twin].face : kHEInvalid;

    removeHEFace(m_halfEdges[he].next);
    if (twin != kHEInvalid) {
        removeHEFace(m_halfEdges[twin].next);
    }

    // Mark the collapsed half-edges themselves invalid.
    m_halfEdges[he].face = kHEInvalid;
    m_halfEdges[he].src  = kHEInvalid;
    m_halfEdges[he].dst  = kHEInvalid;
    if (twin != kHEInvalid) {
        m_halfEdges[twin].face = kHEInvalid;
        m_halfEdges[twin].src  = kHEInvalid;
        m_halfEdges[twin].dst  = kHEInvalid;
    }

    // Mark vDst as removed.
    m_vertices[vDst].outgoing = kHEInvalid;

    // Mark the two removed faces in the face array.
    if (faceA != kHEInvalid) { m_faces[faceA].halfEdge = kHEInvalid; }
    if (faceB != kHEInvalid) { m_faces[faceB].halfEdge = kHEInvalid; }

    // Fix vSrc outgoing to a valid half-edge.
    m_vertices[vSrc].outgoing = kHEInvalid;
    for (uint32_t h = 0; h < static_cast<uint32_t>(m_halfEdges.size()); ++h) {
        if (m_halfEdges[h].src == vSrc && m_halfEdges[h].face != kHEInvalid) {
            m_vertices[vSrc].outgoing = h;
            break;
        }
    }

    return vSrc;
}

} // namespace nexus::geometry
