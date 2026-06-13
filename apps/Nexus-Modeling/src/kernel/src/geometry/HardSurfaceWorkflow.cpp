#include <nexus/geometry/HardSurfaceWorkflow.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

inline Vec3 vec3Add(const Vec3& a, const Vec3& b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 vec3Sub(const Vec3& a, const Vec3& b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 vec3Scale(const Vec3& v, float s) noexcept { return {v.x * s, v.y * s, v.z * s}; }
inline Vec3 vec3Cross(const Vec3& a, const Vec3& b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float vec3Length(const Vec3& v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
inline Vec3 vec3Normalize(const Vec3& v) noexcept {
    float len = vec3Length(v);
    return (len > 1e-12f) ? vec3Scale(v, 1.f / len) : Vec3{0.f, 0.f, 1.f};
}

Vec3 computeFaceNormal(const Face& face, const std::vector<Vec3>& positions) noexcept {
    if (face.indices.size() < 3) return {0.f, 0.f, 1.f};
    const Vec3& p0 = positions[face.indices[0]];
    Vec3 sum{0.f, 0.f, 0.f};
    for (size_t i = 1; i + 1 < face.indices.size(); ++i) {
        sum = vec3Add(sum, vec3Cross(vec3Sub(positions[face.indices[i]], p0),
                                     vec3Sub(positions[face.indices[i + 1]], p0)));
    }
    return vec3Normalize(sum);
}

Vec3 computeFaceCenter(const Face& face, const std::vector<Vec3>& positions) noexcept {
    if (face.indices.empty()) return {0.f, 0.f, 0.f};
    Vec3 sum{0.f, 0.f, 0.f};
    for (auto vi : face.indices) sum = vec3Add(sum, positions[vi]);
    return vec3Scale(sum, 1.f / static_cast<float>(face.indices.size()));
}

} // namespace

// ── init ───────────────────────────────────────────────────────────────────────

void HardSurfaceWorkflow::init(Mesh&& mesh) {
    m_mesh         = std::move(mesh);
    m_originalMesh = m_mesh;
    m_stepReports.clear();
    m_operations.clear();
    m_selectedEdges.clear();
    m_selectedFaces.clear();
    HardSurfaceOperation op;
    op.kind    = HardSurfaceOpKind::Init;
    op.success = true;
    m_operations.push_back(op);
    m_operationCount = 1;

    addReport(HardSurfaceStepKind::Init, "init", true);
}

// ── Selection ──────────────────────────────────────────────────────────────────

void HardSurfaceWorkflow::selectEdge(uint32_t edgeIndex) {
    m_selectedEdges.insert(edgeIndex);
}

void HardSurfaceWorkflow::selectEdges(std::vector<uint32_t> edgeIndices) {
    for (auto ei : edgeIndices) m_selectedEdges.insert(ei);
}

void HardSurfaceWorkflow::selectEdgeLoop(uint32_t seedEdge) {
    if (!m_mesh.isValid()) return;
    auto heOpt = HalfEdgeMesh::fromMesh(m_mesh);
    if (!heOpt) return;

    auto loop = computeEdgeLoop(seedEdge);
    for (auto ei : loop) m_selectedEdges.insert(ei);

    addReport(HardSurfaceStepKind::EdgeSelection,
              "edge loop from " + std::to_string(seedEdge) + " (" + std::to_string(loop.size()) + " edges)",
              !loop.empty());
}

void HardSurfaceWorkflow::selectEdgeRing(uint32_t seedEdge) {
    if (!m_mesh.isValid()) return;
    auto heOpt = HalfEdgeMesh::fromMesh(m_mesh);
    if (!heOpt) return;

    auto ring = computeEdgeRing(seedEdge);
    for (auto ei : ring) m_selectedEdges.insert(ei);

    addReport(HardSurfaceStepKind::EdgeSelection,
              "edge ring from " + std::to_string(seedEdge) + " (" + std::to_string(ring.size()) + " edges)",
              !ring.empty());
}

void HardSurfaceWorkflow::clearEdgeSelection() { m_selectedEdges.clear(); }

void HardSurfaceWorkflow::selectFace(uint32_t faceIndex) {
    m_selectedFaces.insert(faceIndex);
}

void HardSurfaceWorkflow::selectFaces(std::vector<uint32_t> faceIndices) {
    for (auto fi : faceIndices) m_selectedFaces.insert(fi);
}

void HardSurfaceWorkflow::clearFaceSelection() { m_selectedFaces.clear(); }

// ── Topology queries ───────────────────────────────────────────────────────────

void HardSurfaceWorkflow::rebuildHEMIfStale() {
    // HEM is always built fresh for queries — no lazy caching needed
}

std::vector<uint32_t> HardSurfaceWorkflow::computeEdgeLoop(uint32_t seedEdge) const {
    std::vector<uint32_t> result;
    auto heOpt = HalfEdgeMesh::fromMesh(m_mesh);
    if (!heOpt || seedEdge >= heOpt->edgeCount()) return result;

    const auto& hem = *heOpt;
    std::set<uint32_t> seen;
    const uint32_t maxSteps = hem.edgeCount() * 2;

    auto walkDirection = [&](uint32_t start, bool /* forward */) -> std::vector<uint32_t> {
        std::vector<uint32_t> dir;
        uint32_t e = start;
        uint32_t steps = 0;
        while (++steps < maxSteps && !seen.count(e)) {
            seen.insert(e);
            dir.push_back(e);

            uint32_t f = hem.edge(e).face;
            if (f == HalfEdgeMesh::kInvalid) break;

            uint32_t n = hem.edge(e).next;
            uint32_t opp = hem.edge(n).next;
            if (opp == e) break; // triangle — no loop through tris

            uint32_t twin = hem.edge(opp).twin;
            if (twin == HalfEdgeMesh::kInvalid) break;
            if (twin == start) break; // closed loop
            e = twin;
        }
        return dir;
    };

    // Walk in both directions from seed
    auto fwd = walkDirection(seedEdge, true);
    for (auto ei : fwd) result.push_back(ei);

    // Walk backward from seed's previous parallel
    uint32_t e = seedEdge;
    uint32_t f = hem.edge(e).face;
    if (f != HalfEdgeMesh::kInvalid) {
        uint32_t n = hem.edge(hem.edge(e).prev).prev;
        uint32_t twin = hem.edge(n).twin;
        if (twin != HalfEdgeMesh::kInvalid && !seen.count(twin)) {
            auto bwd = walkDirection(twin, false);
            for (size_t i = bwd.size(); i > 0; --i) result.insert(result.begin(), bwd[i - 1]);
        }
    }

    return result;
}

std::vector<uint32_t> HardSurfaceWorkflow::computeEdgeRing(uint32_t seedEdge) const {
    std::vector<uint32_t> result;
    auto heOpt = HalfEdgeMesh::fromMesh(m_mesh);
    if (!heOpt || seedEdge >= heOpt->edgeCount()) return result;

    const auto& hem = *heOpt;
    std::set<uint32_t> seen;
    const uint32_t maxSteps = hem.edgeCount() * 2;

    auto walkRing = [&](uint32_t start) -> std::vector<uint32_t> {
        std::vector<uint32_t> dir;
        uint32_t e = start;
        uint32_t steps = 0;
        while (++steps < maxSteps && !seen.count(e)) {
            seen.insert(e);
            dir.push_back(e);

            uint32_t twin = hem.edge(e).twin;
            if (twin == HalfEdgeMesh::kInvalid) break;

            uint32_t nextInTwin = hem.edge(twin).next;
            if (nextInTwin == HalfEdgeMesh::kInvalid) break;
            if (nextInTwin == start) break;

            e = nextInTwin;
        }
        return dir;
    };

    auto fwd = walkRing(seedEdge);
    for (auto ei : fwd) result.push_back(ei);

    // Walk backward from seed's twin
    uint32_t twin = hem.edge(seedEdge).twin;
    if (twin != HalfEdgeMesh::kInvalid && !seen.count(twin)) {
        uint32_t backStart = hem.edge(twin).prev;
        if (backStart != HalfEdgeMesh::kInvalid && !seen.count(backStart)) {
            auto bwd = walkRing(backStart);
            for (size_t i = bwd.size(); i > 0; --i) result.insert(result.begin(), bwd[i - 1]);
        }
    }

    return result;
}

// ── Operation recording ────────────────────────────────────────────────────────

HardSurfaceWorkflow& HardSurfaceWorkflow::bevelSelectedEdges(float width, int segments) {
    HardSurfaceOperation op;
    op.kind     = HardSurfaceOpKind::BevelSelected;
    op.distance = width;
    op.segments = segments;
    op.success  = false;

    BevelChamferDesc desc;
    desc.mode     = BevelChamferMode::Bevel;
    desc.distance = width;
    op.bevelDesc  = desc;

    m_operations.push_back(op);
    ++m_operationCount;
    addReport(HardSurfaceStepKind::BevelSelected,
              "bevel selected edges width=" + std::to_string(width) + " segs=" + std::to_string(segments),
              false);
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::extrudeSelectedFaces(float distance) {
    HardSurfaceOperation op;
    op.kind     = HardSurfaceOpKind::ExtrudeSelected;
    op.distance = distance;
    op.success  = false;
    m_operations.push_back(op);
    ++m_operationCount;
    addReport(HardSurfaceStepKind::ExtrudeSelected,
              "extrude selected faces dist=" + std::to_string(distance),
              false);
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::insetSelectedFaces(float distance) {
    HardSurfaceOperation op;
    op.kind     = HardSurfaceOpKind::InsetSelected;
    op.distance = distance;
    op.success  = false;
    m_operations.push_back(op);
    ++m_operationCount;
    addReport(HardSurfaceStepKind::InsetSelected,
              "inset selected faces dist=" + std::to_string(distance),
              false);
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::remesh(const RemeshDesc& desc) {
    HardSurfaceOperation op;
    op.kind      = HardSurfaceOpKind::Remesh;
    op.remeshDesc = desc;
    op.success   = false;
    m_operations.push_back(op);
    ++m_operationCount;
    addReport(HardSurfaceStepKind::Remesh,
              "remesh targetEdgeLength=" + std::to_string(desc.targetEdgeLength),
              false);
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::bevelEdges(const BevelChamferDesc& desc) {
    HardSurfaceOperation op;
    op.kind     = HardSurfaceOpKind::BevelChamferLegacy;
    op.bevelDesc = desc;
    op.success  = false;
    m_operations.push_back(op);
    ++m_operationCount;
    addReport(HardSurfaceStepKind::BevelChamfer,
              "bevel chamfer legacy dist=" + std::to_string(desc.distance),
              false);
    return *this;
}

void HardSurfaceWorkflow::rebuildNormals() {
    bool ok = m_mesh.computeVertexNormals();
    HardSurfaceOperation op;
    op.kind    = HardSurfaceOpKind::RebuildNormals;
    op.success = ok;
    m_operations.push_back(op);
    ++m_operationCount;
    addReport(HardSurfaceStepKind::RebuildNormals, "rebuild normals", ok);
}

// ── Modifier-style rebuild ─────────────────────────────────────────────────────

const Mesh& HardSurfaceWorkflow::rebuild() {
    m_mesh = m_originalMesh;
    bool allSuccess = true;

    for (size_t i = 0; i < m_operations.size(); ++i) {
        auto& op = m_operations[i];
        if (op.kind == HardSurfaceOpKind::Init) continue;

        bool ok = false;
        switch (op.kind) {
        case HardSurfaceOpKind::BevelSelected:      ok = applyBevel(op);        break;
        case HardSurfaceOpKind::ExtrudeSelected:     ok = applyExtrude(op);      break;
        case HardSurfaceOpKind::InsetSelected:       ok = applyInset(op);        break;
        case HardSurfaceOpKind::Remesh:              ok = applyRemesh(op);       break;
        case HardSurfaceOpKind::RebuildNormals:
            ok = m_mesh.computeVertexNormals();
            break;
        case HardSurfaceOpKind::BevelChamferLegacy:  ok = applyBevelLegacy(op);  break;
        default: break;
        }
        op.success = ok;
        if (!ok) allSuccess = false;
    }

    if (allSuccess && !m_operations.empty()) {
        addReport(HardSurfaceStepKind::Init, "rebuild complete", true);
    } else if (!allSuccess) {
        addReport(HardSurfaceStepKind::Init, "rebuild complete — some ops failed", false);
    }

    return m_mesh;
}

// ── Internal apply helpers ─────────────────────────────────────────────────────

bool HardSurfaceWorkflow::applyBevel(const HardSurfaceOperation& op) {
    BevelChamferDesc desc = op.bevelDesc;
    if (desc.distance <= 0.f) desc.distance = 0.025f;

    Mesh result;
    auto report = BevelChamferOperation::apply(m_mesh, desc, result);
    if (!report.isSuccess()) return false;

    m_mesh = std::move(result);
    if (desc.recomputeNormals) {
        if (!m_mesh.computeVertexNormals()) return false;
    }
    return true;
}

bool HardSurfaceWorkflow::applyExtrude(const HardSurfaceOperation& op) {
    if (m_selectedFaces.empty()) return false;
    if (!m_mesh.isValid()) return false;

    auto& topo = m_mesh.topology();
    auto& attrs = m_mesh.attributes();
    std::vector<Vec3> positions = attrs.positions();
    // Collect selected face indices and compute per-face normals
    std::vector<uint32_t> selFaces(m_selectedFaces.begin(), m_selectedFaces.end());
    std::map<uint32_t, Vec3> faceNormals;
    for (auto fi : selFaces) {
        if (fi >= topo.faceCount()) continue;
        faceNormals[fi] = computeFaceNormal(topo.face(fi), positions);
    }

    // Duplicate vertices for selected faces, displace along face normal
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> dupMap; // (faceIdx, vertexIdx) -> new vertexIdx
    for (auto fi : selFaces) {
        if (fi >= topo.faceCount()) continue;
        const auto& face = topo.face(fi);
        const Vec3& fn = faceNormals[fi];
        for (auto vi : face.indices) {
            auto key = std::make_pair(fi, vi);
            if (dupMap.count(key)) continue;
            uint32_t newIdx = static_cast<uint32_t>(positions.size());
            positions.push_back(vec3Add(positions[vi], vec3Scale(fn, op.distance)));
            dupMap[key] = newIdx;
        }
    }

    // Rewrite selected faces with displaced vertices; build side faces
    std::vector<Face> newFaces;
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);

        if (m_selectedFaces.count(static_cast<uint32_t>(fi))) {
            Face displaced;
            for (auto vi : face.indices) {
                auto key = std::make_pair(static_cast<uint32_t>(fi), vi);
                displaced.indices.push_back(dupMap.at(key));
            }
            newFaces.push_back(std::move(displaced));
        } else {
            newFaces.push_back(face);
        }
    }

    // Build side faces between original and displaced borders
    for (auto fi : selFaces) {
        if (fi >= topo.faceCount()) continue;
        const auto& face = topo.face(fi);
        const size_t n = face.indices.size();

        for (size_t j = 0; j < n; ++j) {
            uint32_t v0 = face.indices[j];
            uint32_t v1 = face.indices[(j + 1) % n];
            auto key0 = std::make_pair(fi, v0);
            auto key1 = std::make_pair(fi, v1);
            uint32_t d0 = dupMap.at(key0);
            uint32_t d1 = dupMap.at(key1);

            // Check if edge is on boundary of selection
            uint32_t otherFi = static_cast<uint32_t>(-1);
            for (size_t ofi = 0; ofi < topo.faceCount(); ++ofi) {
                if (ofi == fi) continue;
                if (!m_selectedFaces.count(static_cast<uint32_t>(ofi))) continue;
                const auto& oface = topo.face(ofi);
                bool sharesEdge = false;
                for (size_t k = 0; k < oface.indices.size(); ++k) {
                    uint32_t oa = oface.indices[k];
                    uint32_t ob = oface.indices[(k + 1) % oface.indices.size()];
                    if ((oa == v0 && ob == v1) || (oa == v1 && ob == v0)) {
                        sharesEdge = true;
                        break;
                    }
                }
                if (sharesEdge) { otherFi = static_cast<uint32_t>(ofi); break; }
            }

            if (otherFi == static_cast<uint32_t>(-1)) {
                // Boundary edge — create side quad
                Face side;
                side.indices = {v0, v1, d1, d0};
                newFaces.push_back(std::move(side));
            }
        }
    }

    topo.clearFaces();
    for (auto& f : newFaces) topo.addFace(std::move(f));
    attrs.setPositions(std::move(positions));

    m_mesh.rebuildStableElementIds();
    return m_mesh.isValid();
}

bool HardSurfaceWorkflow::applyInset(const HardSurfaceOperation& op) {
    if (m_selectedFaces.empty()) return false;
    if (!m_mesh.isValid()) return false;

    auto& topo = m_mesh.topology();
    auto& attrs = m_mesh.attributes();
    std::vector<Vec3> positions = attrs.positions();
    // Compute face centers and inset each vertex toward center
    std::map<uint32_t, uint32_t> vertDupMap; // origVertex -> insetVertex (per face group)
    std::map<uint32_t, Vec3> faceCenters;
    std::vector<uint32_t> selFaces(m_selectedFaces.begin(), m_selectedFaces.end());

    for (auto fi : selFaces) {
        if (fi >= topo.faceCount()) continue;
        faceCenters[fi] = computeFaceCenter(topo.face(fi), positions);
    }

    // Build inset faces and connecting side faces
    std::vector<Face> newFaces;

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);
        if (!m_selectedFaces.count(static_cast<uint32_t>(fi))) {
            newFaces.push_back(face);
            continue;
        }

        const Vec3& center = faceCenters[static_cast<uint32_t>(fi)];
        Face insetFace;
        for (auto vi : face.indices) {
            Vec3 dir = vec3Sub(positions[vi], center);
            float len = vec3Length(dir);
            if (len < 1e-8f) { insetFace.indices.push_back(vi); continue; }
            float insetLen = std::max(0.f, len - op.distance);
            Vec3 insetPos = vec3Add(center, vec3Scale(vec3Normalize(dir), insetLen));
            uint32_t newIdx = static_cast<uint32_t>(positions.size());
            positions.push_back(insetPos);
            vertDupMap[vi] = newIdx;
            insetFace.indices.push_back(newIdx);
        }
        newFaces.push_back(std::move(insetFace));
    }

    // Build connecting side faces for boundary edges of selection
    for (auto fi : selFaces) {
        if (fi >= topo.faceCount()) continue;
        const auto& face = topo.face(fi);
        const size_t n = face.indices.size();

        for (size_t j = 0; j < n; ++j) {
            uint32_t v0 = face.indices[j];
            uint32_t v1 = face.indices[(j + 1) % n];

            bool isBoundary = true;
            for (auto ofi : selFaces) {
                if (ofi == fi || ofi >= topo.faceCount()) continue;
                const auto& oface = topo.face(ofi);
                for (size_t k = 0; k < oface.indices.size(); ++k) {
                    uint32_t oa = oface.indices[k];
                    uint32_t ob = oface.indices[(k + 1) % oface.indices.size()];
                    if ((oa == v0 && ob == v1) || (oa == v1 && ob == v0)) {
                        isBoundary = false;
                        break;
                    }
                }
                if (!isBoundary) break;
            }

            if (isBoundary && vertDupMap.count(v0) && vertDupMap.count(v1)) {
                uint32_t d0 = vertDupMap[v0];
                uint32_t d1 = vertDupMap[v1];
                Face side;
                side.indices = {v0, v1, d1, d0};
                newFaces.push_back(std::move(side));
            }
        }
    }

    topo.clearFaces();
    for (auto& f : newFaces) topo.addFace(std::move(f));
    attrs.setPositions(std::move(positions));

    m_mesh.rebuildStableElementIds();
    return m_mesh.isValid();
}

bool HardSurfaceWorkflow::applyRemesh(const HardSurfaceOperation& op) {
    Mesh result;
    auto report = RemeshOperation::apply(m_mesh, op.remeshDesc, result);
    if (!report.isSuccess()) return false;

    m_mesh = std::move(result);
    return true;
}

bool HardSurfaceWorkflow::applyBevelLegacy(const HardSurfaceOperation& op) {
    Mesh result;
    auto report = BevelChamferOperation::apply(m_mesh, op.bevelDesc, result);
    if (!report.isSuccess()) return false;

    m_mesh = std::move(result);
    return true;
}

void HardSurfaceWorkflow::addReport(HardSurfaceStepKind kind, std::string msg, bool success) {
    m_stepReports.push_back({kind, std::move(msg), success});
}

} // namespace nexus::geometry
