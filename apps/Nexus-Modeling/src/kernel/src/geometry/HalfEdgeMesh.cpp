#include <nexus/geometry/HalfEdgeMesh.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_set>

namespace nexus::geometry {

// --- fromMesh ----------------------------------------------------------------

std::optional<HalfEdgeMesh> HalfEdgeMesh::fromMesh(const Mesh& mesh) {
    if (!mesh.isValid()) return std::nullopt;

    const auto& attrs = mesh.attributes();
    const auto& topo = mesh.topology();
    const size_t nFaces = topo.faceCount();
    const size_t nVerts = attrs.vertexCount();

    if (nFaces == 0 || nVerts == 0) return std::nullopt;
    if (!topo.allFacesArePoly3Plus()) return std::nullopt;
    if (!topo.hasValidIndices(nVerts)) return std::nullopt;

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() < 3) return std::nullopt;
        std::set<uint32_t> uniq(f.indices.begin(), f.indices.end());
        if (uniq.size() != f.indices.size()) return std::nullopt;
    }

    HalfEdgeMesh hem;
    hem.m_verts.resize(nVerts);
    hem.m_faces.resize(nFaces);
    hem.m_positions = attrs.positions();
    if (attrs.hasUVs()) hem.m_uvs = attrs.uvs();
    if (attrs.hasNormals()) hem.m_normals = attrs.normals();

    std::unordered_map<uint64_t, uint32_t> edgeMap;
    std::vector<std::vector<uint32_t>> faceEdges(nFaces);

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        const uint32_t n = static_cast<uint32_t>(f.indices.size());
        faceEdges[fi].reserve(n);

        for (uint32_t j = 0; j < n; ++j) {
            const uint32_t src = f.indices[j];
            const uint32_t dst = f.indices[(j + 1) % n];

            if (src >= nVerts || dst >= nVerts) return std::nullopt;

            uint32_t heIdx = static_cast<uint32_t>(hem.m_edges.size());
            HEEdge he;
            he.src = src;
            he.face = static_cast<uint32_t>(fi);
            hem.m_edges.push_back(he);
            faceEdges[fi].push_back(heIdx);

            if (hem.m_verts[src].edge == kInvalid) {
                hem.m_verts[src].edge = heIdx;
            }

            uint64_t revKey = (static_cast<uint64_t>(dst) << 32) | static_cast<uint64_t>(src);
            auto twinIt = edgeMap.find(revKey);
            if (twinIt != edgeMap.end()) {
                uint32_t twinIdx = twinIt->second;
                hem.m_edges[heIdx].twin = twinIdx;
                hem.m_edges[twinIdx].twin = heIdx;
                edgeMap.erase(twinIt);
            } else {
                uint64_t fwdKey = (static_cast<uint64_t>(src) << 32) | static_cast<uint64_t>(dst);
                edgeMap[fwdKey] = heIdx;
            }
        }

        for (uint32_t j = 0; j < n; ++j) {
            uint32_t heCurr = faceEdges[fi][j];
            uint32_t heNext = faceEdges[fi][(j + 1) % n];
            uint32_t hePrev = faceEdges[fi][(j + n - 1) % n];
            hem.m_edges[heCurr].next = heNext;
            hem.m_edges[heCurr].prev = hePrev;
        }

        hem.m_faces[fi].edge = faceEdges[fi][0];
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(hem.m_edges.size()); ++i) {
        hem.updateEdgeMap(i);
    }

    return hem;
}

// --- toMesh ------------------------------------------------------------------

Mesh HalfEdgeMesh::toMesh() const {
    Mesh mesh;
    mesh.attributes().setPositions(m_positions);
    if (!m_uvs.empty()) mesh.attributes().setUVs(m_uvs);
    if (!m_normals.empty()) mesh.attributes().setNormals(m_normals);

    for (uint32_t fi = 0; fi < static_cast<uint32_t>(m_faces.size()); ++fi) {
        std::vector<uint32_t> verts;
        uint32_t start = m_faces[fi].edge;
        if (start == kInvalid) continue;

        uint32_t e = start;
        do {
            verts.push_back(m_edges[e].src);
            e = m_edges[e].next;
        } while (e != start);

        for (size_t i = 2; i < verts.size(); ++i) {
            Face f;
            f.indices = {verts[0], verts[static_cast<uint32_t>(i) - 1], verts[i]};
            mesh.topology().addFace(std::move(f));
        }
    }

    return mesh;
}

// --- isManifold --------------------------------------------------------------

bool HalfEdgeMesh::isManifold() const {
    if (m_edges.empty()) return false;

    for (size_t i = 0; i < m_edges.size(); ++i) {
        const auto& e = m_edges[i];
        if (e.twin == kInvalid) return false;
        if (e.twin >= m_edges.size()) return false;
        if (m_edges[e.twin].twin != static_cast<uint32_t>(i)) return false;
    }

    std::vector<bool> edgeVisited(m_edges.size(), false);

    for (uint32_t v = 0; v < static_cast<uint32_t>(m_verts.size()); ++v) {
        uint32_t start = m_verts[v].edge;
        if (start == kInvalid) continue;

        uint32_t walk = start;
        uint32_t visited = 0;
        do {
            if (edgeVisited[walk]) break;
            edgeVisited[walk] = true;
            ++visited;
            uint32_t prevTwin = m_edges[m_edges[walk].prev].twin;
            if (prevTwin == kInvalid) break;
            walk = prevTwin;
        } while (walk != start && visited < m_edges.size());

        if (walk != start) return false;
    }

    return true;
}

// --- isClosed ----------------------------------------------------------------

bool HalfEdgeMesh::isClosed() const {
    for (const auto& e : m_edges) {
        if (e.twin == kInvalid) return false;
    }
    return !m_edges.empty();
}

// --- isTriangulated ----------------------------------------------------------

bool HalfEdgeMesh::isTriangulated() const {
    for (uint32_t fi = 0; fi < static_cast<uint32_t>(m_faces.size()); ++fi) {
        uint32_t start = m_faces[fi].edge;
        if (start == kInvalid) return false;

        uint32_t e = start;
        uint32_t count = 0;
        do {
            ++count;
            e = m_edges[e].next;
            if (count > 100) return false;
        } while (e != start);

        if (count != 3) return false;
    }
    return !m_faces.empty();
}

// --- boundaryLoops -----------------------------------------------------------

std::vector<std::vector<uint32_t>> HalfEdgeMesh::boundaryLoops() const {
    std::vector<std::vector<uint32_t>> loops;
    std::vector<bool> visited(m_edges.size(), false);

    for (uint32_t i = 0; i < static_cast<uint32_t>(m_edges.size()); ++i) {
        if (m_edges[i].twin != kInvalid) continue;
        if (visited[i]) continue;

        std::vector<uint32_t> loop;
        uint32_t e = i;
        uint32_t safety = 0;
        do {
            if (++safety > m_edges.size() * 3) break;
            visited[e] = true;
            loop.push_back(m_edges[e].src);

            e = m_edges[e].next;
            uint32_t twinSafety = 0;
            while (m_edges[e].twin != kInvalid) {
                if (++twinSafety > m_edges.size()) break;
                e = m_edges[m_edges[e].twin].next;
            }
        } while (e != i);

        loops.push_back(std::move(loop));
    }

    return loops;
}

// --- findEdge ----------------------------------------------------------------

uint32_t HalfEdgeMesh::findEdge(uint32_t src, uint32_t dst) const {
    uint64_t key = (static_cast<uint64_t>(src) << 32) | static_cast<uint64_t>(dst);
    auto it = m_edgeMap.find(key);
    if (it != m_edgeMap.end()) {
        uint32_t ei = it->second;
        const auto& e = m_edges[ei];
        if (e.face != kInvalid && e.next != kInvalid && m_edges[e.next].src == dst) {
            return ei;
        }
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_edges.size()); ++i) {
        if (m_edges[i].src != src) continue;
        if (m_edges[i].face == kInvalid) continue;
        if (m_edges[i].next == kInvalid) continue;
        if (m_edges[m_edges[i].next].src == dst) return i;
    }
    return kInvalid;
}

// --- vertexFaces -------------------------------------------------------------

std::vector<uint32_t> HalfEdgeMesh::vertexFaces(uint32_t v) const {
    std::vector<uint32_t> faces;
    if (v >= m_verts.size()) return faces;

    uint32_t start = m_verts[v].edge;
    if (start == kInvalid) return faces;

    uint32_t e = start;
    uint32_t safety = 0;
    do {
        if (m_edges[e].face != kInvalid) {
            faces.push_back(m_edges[e].face);
        }
        uint32_t prevTwin = m_edges[m_edges[e].prev].twin;
        if (prevTwin == kInvalid) break;
        e = prevTwin;
        if (++safety > m_edges.size() * 2) break;
    } while (e != start);

    return faces;
}

// --- vertexNeighbors ---------------------------------------------------------

std::vector<uint32_t> HalfEdgeMesh::vertexNeighbors(uint32_t v) const {
    std::vector<uint32_t> neighbors;
    if (v >= m_verts.size()) return neighbors;

    uint32_t start = m_verts[v].edge;
    if (start == kInvalid) return neighbors;

    uint32_t e = start;
    uint32_t safety = 0;
    do {
        neighbors.push_back(m_edges[m_edges[e].next].src);
        uint32_t prevTwin = m_edges[m_edges[e].prev].twin;
        if (prevTwin == kInvalid) break;
        e = prevTwin;
        if (++safety > m_edges.size() * 2) break;
    } while (e != start);

    return neighbors;
}

// --- flipEdge ----------------------------------------------------------------

bool HalfEdgeMesh::flipEdge(uint32_t he) {
    if (he >= m_edges.size()) return false;

    uint32_t t = m_edges[he].twin;
    if (t == kInvalid || t >= m_edges.size()) return false;

    uint32_t a = he;
    uint32_t b = m_edges[a].next;
    uint32_t c = m_edges[a].prev;

    uint32_t d = t;
    uint32_t e = m_edges[d].next;
    uint32_t f = m_edges[d].prev;

    uint32_t v0 = m_edges[a].src;
    uint32_t v1 = m_edges[b].src;
    uint32_t v2 = m_edges[c].src;
    uint32_t v3 = m_edges[f].src;

    if (findEdge(v2, v3) != kInvalid || findEdge(v3, v2) != kInvalid) return false;

    uint32_t countA = 0, countB = 0;
    uint32_t ee = a;
    do { ++countA; ee = m_edges[ee].next; } while (ee != a);
    ee = d;
    do { ++countB; ee = m_edges[ee].next; } while (ee != d);
    if (countA != 3 || countB != 3) return false;

    uint32_t fA = m_edges[a].face;
    uint32_t fB = m_edges[d].face;

    m_edges[a].src = v2;
    m_edges[d].src = v3;

    m_edges[d].next = c;   m_edges[d].prev = e;   m_edges[d].face = fA;
    m_edges[c].next = e;   m_edges[c].prev = d;   m_edges[c].face = fA;
    m_edges[e].next = d;   m_edges[e].prev = c;   m_edges[e].face = fA;

    m_edges[a].next = f;   m_edges[a].prev = b;   m_edges[a].face = fB;
    m_edges[f].next = b;   m_edges[f].prev = a;   m_edges[f].face = fB;
    m_edges[b].next = a;   m_edges[b].prev = f;   m_edges[b].face = fB;

    m_edges[a].twin = d;
    m_edges[d].twin = a;

    m_faces[fA].edge = d;
    m_faces[fB].edge = a;

    if (m_verts[v0].edge == a || m_verts[v0].edge == c) m_verts[v0].edge = e;
    if (m_verts[v1].edge == b || m_verts[v1].edge == d) m_verts[v1].edge = b;
    if (m_verts[v2].edge == c) m_verts[v2].edge = a;
    if (m_verts[v3].edge == f) m_verts[v3].edge = d;

    updateEdgeMap(a);
    updateEdgeMap(d);

    return true;
}

// --- splitEdge ---------------------------------------------------------------

bool HalfEdgeMesh::splitEdge(uint32_t he) {
    if (he >= m_edges.size()) return false;

    uint32_t t = m_edges[he].twin;
    if (t == kInvalid || t >= m_edges.size()) return false;

    uint32_t a = he;
    uint32_t b = m_edges[a].next;
    uint32_t c = m_edges[a].prev;

    uint32_t d = t;
    uint32_t e = m_edges[d].next;
    uint32_t f = m_edges[d].prev;

    uint32_t v0 = m_edges[a].src;
    uint32_t v1 = m_edges[b].src;
    uint32_t v2 = m_edges[c].src;
    uint32_t v3 = m_edges[f].src;

    uint32_t countA = 0, countB = 0;
    uint32_t ee = a;
    do { ++countA; ee = m_edges[ee].next; } while (ee != a);
    ee = d;
    do { ++countB; ee = m_edges[ee].next; } while (ee != d);
    if (countA != 3 || countB != 3) return false;

    const auto& p0 = m_positions[v0];
    const auto& p1 = m_positions[v1];
    nexus::render::Vec3 mid = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f, (p0.z + p1.z) * 0.5f};
    uint32_t vMid = static_cast<uint32_t>(m_positions.size());
    m_positions.push_back(mid);
    m_verts.push_back({});

    uint32_t fA = m_edges[a].face;
    uint32_t fB = m_edges[d].face;

    uint32_t fA0 = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});
    uint32_t fA1 = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});
    uint32_t fB0 = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});
    uint32_t fB1 = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});

    uint32_t eA0_0 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t eA0_1 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t etA0_0 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t etA0_1 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t eA1_0 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t etA1_0 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t eB0_1 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t etB0_1 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});

    m_edges[eA0_0] = {eA0_1, c, etA0_0, v0, fA0};
    m_edges[eA0_1] = {c, eA0_0, etA0_1, vMid, fA0};
    m_edges[c].next = eA0_0; m_edges[c].prev = eA0_1; m_edges[c].face = fA0;

    m_edges[etA0_0] = {e, eB0_1, eA0_0, vMid, fB1};
    m_edges[etA0_1] = {eA1_0, b, eA0_1, v2, fA1};

    m_edges[eA1_0] = {b, etA0_1, etA1_0, vMid, fA1};
    m_edges[b].next = etA0_1; m_edges[b].prev = eA1_0; m_edges[b].face = fA1;

    m_edges[etA1_0] = {eB0_1, f, eA1_0, v1, fB0};

    m_edges[eB0_1] = {f, etA1_0, etB0_1, vMid, fB0};
    m_edges[f].next = etA1_0; m_edges[f].prev = eB0_1; m_edges[f].face = fB0;

    m_edges[etB0_1] = {etA0_0, e, eB0_1, v3, fB1};
    m_edges[e].next = etB0_1; m_edges[e].prev = etA0_0; m_edges[e].face = fB1;

    m_faces[fA].edge = kInvalid;
    m_faces[fB].edge = kInvalid;

    m_faces[fA0].edge = eA0_0;
    m_faces[fA1].edge = eA1_0;
    m_faces[fB0].edge = etA1_0;
    m_faces[fB1].edge = etA0_0;

    m_edges[a].face = kInvalid;
    m_edges[d].face = kInvalid;

    m_verts[vMid].edge = eA0_1;
    if (m_verts[v0].edge == a) m_verts[v0].edge = eA0_0;
    if (m_verts[v1].edge == d || m_verts[v1].edge == b) m_verts[v1].edge = etA1_0;

    updateEdgeMap(eA0_0);
    updateEdgeMap(eA0_1);
    updateEdgeMap(etA0_0);
    updateEdgeMap(etA0_1);
    updateEdgeMap(eA1_0);
    updateEdgeMap(etA1_0);
    updateEdgeMap(eB0_1);
    updateEdgeMap(etB0_1);

    return true;
}

// --- collapseEdge ------------------------------------------------------------

bool HalfEdgeMesh::collapseEdge(uint32_t he, const nexus::render::Vec3& target) {
    if (he >= m_edges.size()) return false;

    uint32_t t = m_edges[he].twin;
    if (t == kInvalid || t >= m_edges.size()) return false;

    uint32_t a = he;
    uint32_t b = m_edges[a].next;
    uint32_t c = m_edges[a].prev;

    uint32_t d = t;
    uint32_t e = m_edges[d].next;
    uint32_t f = m_edges[d].prev;

    uint32_t vKeep = m_edges[d].src;
    uint32_t vRemove = m_edges[a].src;
    uint32_t vl = m_edges[c].src;
    uint32_t vr = m_edges[f].src;

    if (vKeep == vRemove) return false;

    std::unordered_set<uint32_t> neighborsRemove;
    {
        uint32_t start = m_verts[vRemove].edge;
        if (start == kInvalid) return false;
        uint32_t walk = start;
        do {
            neighborsRemove.insert(m_edges[m_edges[walk].next].src);
            uint32_t prevTwin = m_edges[m_edges[walk].prev].twin;
            if (prevTwin == kInvalid) break;
            walk = prevTwin;
        } while (walk != start);
    }

    std::unordered_set<uint32_t> neighborsKeep;
    {
        uint32_t start = m_verts[vKeep].edge;
        if (start == kInvalid) return false;
        uint32_t walk = start;
        do {
            neighborsKeep.insert(m_edges[m_edges[walk].next].src);
            uint32_t prevTwin = m_edges[m_edges[walk].prev].twin;
            if (prevTwin == kInvalid) break;
            walk = prevTwin;
        } while (walk != start);
    }

    for (uint32_t n : neighborsRemove) {
        if (n == vl || n == vr) continue;
        if (neighborsKeep.count(n)) return false;
    }
    for (uint32_t n : neighborsKeep) {
        if (n == vl || n == vr) continue;
        if (neighborsRemove.count(n)) return false;
    }

    m_positions[vKeep] = target;

    // Save face indices before killEdge destroys them.
    uint32_t faceA = m_edges[he].face;
    uint32_t faceB = m_edges[t].face;

    for (auto& edge : m_edges) {
        if (edge.src == vRemove) edge.src = vKeep;
    }

    // Rebuild edgeMap entries for every surviving edge whose src changed.
    for (uint32_t ei = 0; ei < static_cast<uint32_t>(m_edges.size()); ++ei) {
        updateEdgeMap(ei);
    }

    auto killEdge = [&](uint32_t ei) {
        if (ei < m_edges.size()) {
            m_edges[ei].face = kInvalid;
            m_edges[ei].next = kInvalid;
            m_edges[ei].prev = kInvalid;
            m_edges[ei].twin = kInvalid;
            m_edges[ei].src = kInvalid;
        }
    };

    killEdge(a); killEdge(b); killEdge(c);
    killEdge(d); killEdge(e); killEdge(f);

    for (uint32_t eid : {a, b, c, d, e, f}) {
        if (eid < static_cast<uint32_t>(m_edges.size())) {
            const auto& edge = m_edges[eid];
            if (edge.src != kInvalid) {
                uint32_t dst = kInvalid;
                if (edge.next != kInvalid && edge.next < m_edges.size()) {
                    dst = m_edges[edge.next].src;
                }
                if (dst != kInvalid) {
                    uint64_t key = (static_cast<uint64_t>(edge.src) << 32) | static_cast<uint64_t>(dst);
                    m_edgeMap.erase(key);
                }
            }
        }
    }

    if (faceA < m_faces.size()) m_faces[faceA].edge = kInvalid;
    if (faceB < m_faces.size()) m_faces[faceB].edge = kInvalid;

    if (m_verts[vKeep].edge == a || m_verts[vKeep].edge == b ||
        m_verts[vKeep].edge == c || m_verts[vKeep].edge == d ||
        m_verts[vKeep].edge == e || m_verts[vKeep].edge == f) {
        m_verts[vKeep].edge = kInvalid;
        for (uint32_t ei = 0; ei < static_cast<uint32_t>(m_edges.size()); ++ei) {
            if (m_edges[ei].src == vKeep && m_edges[ei].face != kInvalid && m_edges[ei].next != kInvalid) {
                m_verts[vKeep].edge = ei;
                break;
            }
        }
    }

    m_verts[vRemove].edge = kInvalid;

    return true;
}

// --- updateEdgeMap -----------------------------------------------------------

void HalfEdgeMesh::updateEdgeMap(uint32_t ei) {
    const auto& e = m_edges[ei];
    if (e.src == kInvalid || e.src >= m_verts.size()) return;

    uint32_t dst = kInvalid;
    if (e.next != kInvalid && e.next < m_edges.size()) {
        dst = m_edges[e.next].src;
    } else if (e.twin != kInvalid && e.twin < m_edges.size()) {
        dst = m_edges[e.twin].src;
    }

    if (dst == kInvalid) return;

    uint64_t key = (static_cast<uint64_t>(e.src) << 32) | static_cast<uint64_t>(dst);
    m_edgeMap[key] = ei;
}

// --- addEdgePair -------------------------------------------------------------

uint32_t HalfEdgeMesh::addEdgePair(uint32_t src, uint32_t dst, uint32_t face) {
    uint32_t idx0 = static_cast<uint32_t>(m_edges.size());
    HEEdge e0;
    e0.src = src;
    e0.face = face;
    m_edges.push_back(e0);

    uint32_t idx1 = static_cast<uint32_t>(m_edges.size());
    HEEdge e1;
    e1.src = dst;
    e1.face = face;
    m_edges.push_back(e1);

    m_edges[idx0].twin = idx1;
    m_edges[idx1].twin = idx0;

    updateEdgeMap(idx0);
    updateEdgeMap(idx1);

    return idx0;
}

} // namespace nexus::geometry
