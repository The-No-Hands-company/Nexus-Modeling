#include <nexus/geometry/SubdivisionSurface.h>

#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace nexus::geometry {

namespace {

using EdgeKey = std::pair<uint32_t, uint32_t>;

static EdgeKey makeKey(uint32_t a, uint32_t b) noexcept
{
    return (a < b) ? EdgeKey{a, b} : EdgeKey{b, a};
}

// ─── Loop subdivision (one level) ─────────────────────────────────────────────

static std::optional<HalfEdgeMesh> loopOneLevel(const HalfEdgeMesh& mesh,
                                                  bool smoothBoundary)
{
    const auto& hes  = mesh.halfEdges();
    const auto& verts = mesh.vertices();
    const auto& faces = mesh.faces();

    const uint32_t nV = mesh.vertexCount();
    const uint32_t nF = mesh.faceCount();

    // Verify all faces are triangles.
    for (uint32_t fi = 0; fi < nF; ++fi) {
        if (faces[fi].halfEdge == kHEInvalid) { continue; }
        auto fv = mesh.faceVertices(fi);
        if (fv.size() != 3) { return std::nullopt; }
    }

    // Step 1: compute even (original) vertex positions.
    std::vector<nexus::render::Vec3> newPos(nV);
    for (uint32_t vi = 0; vi < nV; ++vi) {
        const auto& vr = verts[vi];
        if (vr.outgoing == kHEInvalid) {
            newPos[vi] = vr.position;
            continue;
        }

        // Collect one-ring neighbours.
        std::vector<uint32_t> ring;
        {
            const uint32_t start = vr.outgoing;
            uint32_t cur = start;
            uint32_t guard = 0;
            bool hitBoundary = false;
            do {
                ring.push_back(hes[cur].dst);
                const uint32_t tw = hes[cur].twin;
                if (tw == kHEInvalid) { hitBoundary = true; break; }
                cur = hes[tw].next;
                if (++guard > hes.size()) { hitBoundary = true; break; }
            } while (cur != start);

            if (hitBoundary) {
                // Also walk backward to collect the other side.
                cur = hes[start].prev;
                guard = 0;
                while (true) {
                    ring.push_back(hes[cur].src);
                    const uint32_t tw = hes[cur].twin;
                    if (tw == kHEInvalid) { break; }
                    cur = hes[tw].prev;
                    if (++guard > hes.size()) { break; }
                }
            }
        }

        const uint32_t n = static_cast<uint32_t>(ring.size());
        if (n == 0) { newPos[vi] = vr.position; continue; }

        // Check if boundary vertex (any outgoing HE has no twin).
        bool isBoundary = false;
        {
            const uint32_t start = vr.outgoing;
            uint32_t cur = start;
            uint32_t guard = 0;
            do {
                const uint32_t tw = hes[cur].twin;
                if (tw == kHEInvalid) { isBoundary = true; break; }
                cur = hes[tw].next;
                if (++guard > hes.size()) { break; }
            } while (cur != start);
        }

        if (isBoundary) {
            if (smoothBoundary) {
                // Boundary stencil: 3/4 * v + 1/8 * (v_prev + v_next)
                // Find the two boundary neighbours.
                nexus::render::Vec3 bSum{0, 0, 0};
                uint32_t bCount = 0;
                for (uint32_t ni : ring) {
                    // Check if the edge to ni is a boundary edge.
                    const uint32_t start = vr.outgoing;
                    uint32_t cur2 = start;
                    uint32_t g2 = 0;
                    do {
                        if (hes[cur2].dst == ni && hes[cur2].twin == kHEInvalid) {
                            bSum.x += verts[ni].position.x;
                            bSum.y += verts[ni].position.y;
                            bSum.z += verts[ni].position.z;
                            ++bCount;
                            break;
                        }
                        uint32_t tw = hes[cur2].twin;
                        if (tw == kHEInvalid) { break; }
                        cur2 = hes[tw].next;
                        if (++g2 > hes.size()) { break; }
                    } while (cur2 != start);
                }
                if (bCount == 2) {
                    newPos[vi] = {
                        0.75f * vr.position.x + 0.125f * bSum.x,
                        0.75f * vr.position.y + 0.125f * bSum.y,
                        0.75f * vr.position.z + 0.125f * bSum.z};
                } else {
                    newPos[vi] = vr.position;
                }
            } else {
                newPos[vi] = vr.position; // crease rule: boundary fixed
            }
        } else {
            // Interior Loop stencil: beta = 3/16 for n==3, else (1/n)(5/8-(3/8+cos(2π/n)/4)²)
            double beta;
            if (n == 3) {
                beta = 3.0 / 16.0;
            } else {
                double t = 3.0 / 8.0 + std::cos(2.0 * M_PI / n) / 4.0;
                beta = (1.0 / n) * (5.0 / 8.0 - t * t);
            }
            nexus::render::Vec3 ringSum{0, 0, 0};
            for (uint32_t ni : ring) {
                ringSum.x += verts[ni].position.x;
                ringSum.y += verts[ni].position.y;
                ringSum.z += verts[ni].position.z;
            }
            const float w  = static_cast<float>(1.0 - n * beta);
            const float wb = static_cast<float>(beta);
            newPos[vi] = {
                w * vr.position.x + wb * ringSum.x,
                w * vr.position.y + wb * ringSum.y,
                w * vr.position.z + wb * ringSum.z};
        }
    }

    // Step 2: compute odd (edge midpoint) vertex positions.
    // Map each undirected edge to its new vertex index.
    std::map<EdgeKey, uint32_t> edgeMidIdx;
    std::vector<nexus::render::Vec3> midPositions;

    for (uint32_t hi = 0; hi < static_cast<uint32_t>(hes.size()); ++hi) {
        const auto& he = hes[hi];
        if (he.src == kHEInvalid || he.dst == kHEInvalid) { continue; }
        if (he.face == kHEInvalid) { continue; }
        const EdgeKey ek = makeKey(he.src, he.dst);
        if (edgeMidIdx.count(ek)) { continue; }

        const uint32_t midIdx = nV + static_cast<uint32_t>(midPositions.size());
        edgeMidIdx[ek] = midIdx;

        const nexus::render::Vec3& p0 = verts[he.src].position;
        const nexus::render::Vec3& p1 = verts[he.dst].position;

        bool boundary = (he.twin == kHEInvalid);
        nexus::render::Vec3 mp{};
        if (boundary || !smoothBoundary) {
            mp = {(p0.x + p1.x) * 0.5f,
                  (p0.y + p1.y) * 0.5f,
                  (p0.z + p1.z) * 0.5f};
        } else {
            // Interior Loop edge stencil: 3/8*(v0+v1) + 1/8*(opp0+opp1)
            // opp0 = third vertex of he's face, opp1 = third vertex of twin's face
            uint32_t opp0 = hes[he.next].dst;
            uint32_t opp1 = hes[hes[he.twin].next].dst;
            const nexus::render::Vec3& q0 = verts[opp0].position;
            const nexus::render::Vec3& q1 = verts[opp1].position;
            mp = {
                0.375f * (p0.x + p1.x) + 0.125f * (q0.x + q1.x),
                0.375f * (p0.y + p1.y) + 0.125f * (q0.y + q1.y),
                0.375f * (p0.z + p1.z) + 0.125f * (q0.z + q1.z)};
        }
        midPositions.push_back(mp);
    }

    // Step 3: build the new mesh.
    Mesh newMesh;
    MeshAttributes attrs;
    std::vector<nexus::render::Vec3> allPos;
    allPos.reserve(nV + midPositions.size());
    for (uint32_t vi = 0; vi < nV; ++vi) { allPos.push_back(newPos[vi]); }
    for (auto& mp : midPositions)         { allPos.push_back(mp); }
    attrs.setPositions(std::move(allPos));
    newMesh.attributes() = std::move(attrs);

    // Each original triangle (v0, v1, v2) → 4 triangles:
    //   (v0, m01, m20), (v1, m12, m01), (v2, m20, m12), (m01, m12, m20)
    for (uint32_t fi = 0; fi < nF; ++fi) {
        if (faces[fi].halfEdge == kHEInvalid) { continue; }
        auto fv = mesh.faceVertices(fi);
        if (fv.size() != 3) { continue; }
        const uint32_t v0 = fv[0], v1 = fv[1], v2 = fv[2];
        const uint32_t m01 = edgeMidIdx.at(makeKey(v0, v1));
        const uint32_t m12 = edgeMidIdx.at(makeKey(v1, v2));
        const uint32_t m20 = edgeMidIdx.at(makeKey(v2, v0));

        Face f0; f0.indices = {v0, m01, m20};
        Face f1; f1.indices = {v1, m12, m01};
        Face f2; f2.indices = {v2, m20, m12};
        Face f3; f3.indices = {m01, m12, m20};
        newMesh.topology().addFace(std::move(f0));
        newMesh.topology().addFace(std::move(f1));
        newMesh.topology().addFace(std::move(f2));
        newMesh.topology().addFace(std::move(f3));
    }

    return HalfEdgeMesh::fromMesh(newMesh);
}

// ─── Catmull-Clark (one level) ────────────────────────────────────────────────

static std::optional<HalfEdgeMesh> catmullClarkOneLevel(const HalfEdgeMesh& mesh,
                                                          bool smoothBoundary)
{
    const auto& hes   = mesh.halfEdges();
    const auto& verts = mesh.vertices();
    const auto& faces = mesh.faces();

    const uint32_t nV = mesh.vertexCount();
    const uint32_t nF = mesh.faceCount();
    const uint32_t nHE = mesh.halfEdgeCount();

    // Step 1: face points — average of face vertices.
    std::vector<nexus::render::Vec3> facePoints(nF, {0, 0, 0});
    std::vector<uint32_t> faceDegree(nF, 0);
    for (uint32_t fi = 0; fi < nF; ++fi) {
        if (faces[fi].halfEdge == kHEInvalid) { continue; }
        auto fv = mesh.faceVertices(fi);
        for (uint32_t vi : fv) {
            facePoints[fi].x += verts[vi].position.x;
            facePoints[fi].y += verts[vi].position.y;
            facePoints[fi].z += verts[vi].position.z;
        }
        faceDegree[fi] = static_cast<uint32_t>(fv.size());
        if (faceDegree[fi] > 0) {
            const float inv = 1.f / faceDegree[fi];
            facePoints[fi].x *= inv;
            facePoints[fi].y *= inv;
            facePoints[fi].z *= inv;
        }
    }

    // Step 2: edge points.
    // Edge point = (v0 + v1 + fp0 + fp1) / 4 for interior edges
    //            = (v0 + v1) / 2 for boundary edges.
    std::map<EdgeKey, uint32_t> edgePtIdx; // edge → new vertex index (nV + nF + seq)
    std::vector<nexus::render::Vec3> edgePtPositions;

    for (uint32_t hi = 0; hi < nHE; ++hi) {
        const auto& he = hes[hi];
        if (he.src == kHEInvalid || he.dst == kHEInvalid) { continue; }
        if (he.face == kHEInvalid) { continue; }
        const EdgeKey ek = makeKey(he.src, he.dst);
        if (edgePtIdx.count(ek)) { continue; }

        const uint32_t idx = nV + nF + static_cast<uint32_t>(edgePtPositions.size());
        edgePtIdx[ek] = idx;

        const nexus::render::Vec3& p0 = verts[he.src].position;
        const nexus::render::Vec3& p1 = verts[he.dst].position;

        if (he.twin == kHEInvalid || !smoothBoundary) {
            edgePtPositions.push_back({
                (p0.x + p1.x) * 0.5f,
                (p0.y + p1.y) * 0.5f,
                (p0.z + p1.z) * 0.5f});
        } else {
            const nexus::render::Vec3& fp0 = facePoints[he.face];
            const nexus::render::Vec3& fp1 = facePoints[hes[he.twin].face];
            edgePtPositions.push_back({
                (p0.x + p1.x + fp0.x + fp1.x) * 0.25f,
                (p0.y + p1.y + fp0.y + fp1.y) * 0.25f,
                (p0.z + p1.z + fp0.z + fp1.z) * 0.25f});
        }
    }

    // Step 3: new vertex positions (Catmull-Clark stencil).
    // For interior vertex of valence n:
    //   F = average of adjacent face points
    //   E = average of adjacent edge midpoints
    //   new = (F + 2E + (n-3)*V) / n
    std::vector<nexus::render::Vec3> newVPos(nV);
    for (uint32_t vi = 0; vi < nV; ++vi) {
        const auto& vr = verts[vi];
        if (vr.outgoing == kHEInvalid) { newVPos[vi] = vr.position; continue; }

        // Collect incident faces and edge midpoints.
        auto incFaces = mesh.vertexFaces(vi);
        const uint32_t n = static_cast<uint32_t>(incFaces.size());
        if (n == 0) { newVPos[vi] = vr.position; continue; }

        // Check boundary.
        bool isBoundary = false;
        {
            const uint32_t start = vr.outgoing;
            uint32_t cur = start;
            uint32_t guard = 0;
            do {
                if (hes[cur].twin == kHEInvalid) { isBoundary = true; break; }
                cur = hes[hes[cur].twin].next;
                if (++guard > hes.size()) { break; }
            } while (cur != start);
        }

        if (isBoundary || !smoothBoundary) {
            newVPos[vi] = vr.position;
            continue;
        }

        nexus::render::Vec3 F{0, 0, 0};
        for (uint32_t fi : incFaces) {
            F.x += facePoints[fi].x;
            F.y += facePoints[fi].y;
            F.z += facePoints[fi].z;
        }
        F.x /= n; F.y /= n; F.z /= n;

        // Edge midpoints: walk the one-ring and take midpoints.
        nexus::render::Vec3 E{0, 0, 0};
        uint32_t eCnt = 0;
        {
            const uint32_t start = vr.outgoing;
            uint32_t cur = start;
            uint32_t guard = 0;
            do {
                const nexus::render::Vec3& p0 = vr.position;
                const nexus::render::Vec3& p1 = verts[hes[cur].dst].position;
                E.x += (p0.x + p1.x) * 0.5f;
                E.y += (p0.y + p1.y) * 0.5f;
                E.z += (p0.z + p1.z) * 0.5f;
                ++eCnt;
                const uint32_t tw = hes[cur].twin;
                if (tw == kHEInvalid) { break; }
                cur = hes[tw].next;
                if (++guard > hes.size()) { break; }
            } while (cur != start);
        }
        if (eCnt > 0) { E.x /= eCnt; E.y /= eCnt; E.z /= eCnt; }

        const float fn = static_cast<float>(n);
        newVPos[vi] = {
            (F.x + 2.f * E.x + (fn - 3.f) * vr.position.x) / fn,
            (F.y + 2.f * E.y + (fn - 3.f) * vr.position.y) / fn,
            (F.z + 2.f * E.z + (fn - 3.f) * vr.position.z) / fn};
    }

    // Step 4: build the new mesh.
    // Each original face with vertices [v0, v1, ..., vk] and face point F
    // becomes k quads: (vi, edge(vi, vi+1), F, edge(vi-1, vi)) for each i.
    Mesh newMesh;
    MeshAttributes attrs;
    std::vector<nexus::render::Vec3> allPos;
    allPos.reserve(nV + nF + edgePtPositions.size());
    for (uint32_t vi = 0; vi < nV; ++vi) { allPos.push_back(newVPos[vi]); }
    for (uint32_t fi = 0; fi < nF; ++fi) { allPos.push_back(facePoints[fi]); }
    for (auto& ep : edgePtPositions)      { allPos.push_back(ep); }
    attrs.setPositions(std::move(allPos));
    newMesh.attributes() = std::move(attrs);

    for (uint32_t fi = 0; fi < nF; ++fi) {
        if (faces[fi].halfEdge == kHEInvalid) { continue; }
        auto fv = mesh.faceVertices(fi);
        const uint32_t deg = static_cast<uint32_t>(fv.size());
        if (deg < 3) { continue; }
        const uint32_t fpIdx = nV + fi; // face point index in allPos

        for (uint32_t i = 0; i < deg; ++i) {
            const uint32_t vi   = fv[i];
            const uint32_t vnxt = fv[(i + 1) % deg];
            const uint32_t vprv = fv[(i + deg - 1) % deg];

            const uint32_t eCur  = edgePtIdx.at(makeKey(vi, vnxt));
            const uint32_t ePrev = edgePtIdx.at(makeKey(vprv, vi));

            Face q;
            q.indices = {vi, eCur, fpIdx, ePrev};
            newMesh.topology().addFace(std::move(q));
        }
    }

    return HalfEdgeMesh::fromMesh(newMesh);
}

} // anonymous namespace

// ─── Public API ──────────────────────────────────────────────────────────────

std::optional<HalfEdgeMesh> SubdivisionSurface::loopSubdivide(
    const HalfEdgeMesh& mesh, const SubdivisionOptions& opts)
{
    if (!mesh.isTriangulated()) { return std::nullopt; }
    std::optional<HalfEdgeMesh> cur = mesh;
    for (uint32_t lv = 0; lv < opts.levels; ++lv) {
        cur = loopOneLevel(*cur, opts.smoothBoundary);
        if (!cur) { return std::nullopt; }
    }
    return cur;
}

std::optional<HalfEdgeMesh> SubdivisionSurface::catmullClark(
    const HalfEdgeMesh& mesh, const SubdivisionOptions& opts)
{
    std::optional<HalfEdgeMesh> cur = mesh;
    for (uint32_t lv = 0; lv < opts.levels; ++lv) {
        cur = catmullClarkOneLevel(*cur, opts.smoothBoundary);
        if (!cur) { return std::nullopt; }
    }
    return cur;
}

} // namespace nexus::geometry
