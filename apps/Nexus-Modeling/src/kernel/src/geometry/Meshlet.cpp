#include <nexus/geometry/Meshlet.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <random>
#include <unordered_map>
#include <unordered_set>

namespace nexus::geometry {

namespace {

// Undirected edge key
struct EdgeKey {
    uint32_t lo, hi;
    EdgeKey(uint32_t a, uint32_t b) : lo(std::min(a, b)), hi(std::max(a, b)) {}
    bool operator==(const EdgeKey& o) const { return lo == o.lo && hi == o.hi; }
};
struct EdgeKeyHash {
    size_t operator()(const EdgeKey& k) const {
        return static_cast<size_t>((static_cast<uint64_t>(k.hi) << 32) | k.lo);
    }
};

} // anonymous namespace

// --- build --------------------------------------------------------------------

MeshletResult MeshletBuilder::build(const Mesh& mesh, uint32_t maxVerts, uint32_t maxTris) {
    MeshletResult result;

    if (!mesh.isValid()) {
        result.error = "Invalid mesh";
        return result;
    }

    const auto& topo = mesh.topology();
    const size_t nFaces = topo.faceCount();

    if (nFaces == 0) {
        result.ok = true;
        return result;
    }

    // --- Collect triangulated faces -------------------------------------------
    // Build vertex-to-face adjacency and edge-to-face adjacency
    struct TriFace { uint32_t v0, v1, v2; };
    std::vector<TriFace> triFaces;

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() < 3) continue;

        // Fan-triangulate polygon faces
        for (size_t i = 2; i < f.indices.size(); ++i) {
            triFaces.push_back({f.indices[0], f.indices[static_cast<uint32_t>(i) - 1],
                                f.indices[i]});
        }
    }

    if (triFaces.empty()) {
        result.ok = true;
        return result;
    }

    const size_t nTris = triFaces.size();

    // --- Build edge adjacency -------------------------------------------------
    std::unordered_map<EdgeKey, std::vector<size_t>, EdgeKeyHash> edgeToTri;
    for (size_t ti = 0; ti < nTris; ++ti) {
        const auto& t = triFaces[ti];
        uint32_t es[3][2] = {{t.v0, t.v1}, {t.v1, t.v2}, {t.v2, t.v0}};
        for (int e = 0; e < 3; ++e) {
            edgeToTri[EdgeKey(es[e][0], es[e][1])].push_back(ti);
        }
    }

    // --- Build triangle-to-neighbor adjacency ---------------------------------
    std::vector<std::vector<size_t>> triAdj(nTris);
    for (auto& [ekey, tlist] : edgeToTri) {
        for (size_t i = 0; i < tlist.size(); ++i) {
            for (size_t j = i + 1; j < tlist.size(); ++j) {
                triAdj[tlist[i]].push_back(tlist[j]);
                triAdj[tlist[j]].push_back(tlist[i]);
            }
        }
    }

    // --- Greedy meshlet formation ---------------------------------------------
    std::vector<bool> assigned(nTris, false);
    size_t firstUnassigned = 0;

    while (firstUnassigned < nTris) {
        // Find next unassigned triangle
        while (firstUnassigned < nTris && assigned[firstUnassigned]) ++firstUnassigned;
        if (firstUnassigned >= nTris) break;

        size_t seed = firstUnassigned;

        MeshletData meshlet;
        std::unordered_map<uint32_t, uint32_t> globalToLocal;
        std::vector<size_t> meshletTris;

        // Add seed triangle
        {
            const auto& t = triFaces[seed];
            auto addVert = [&](uint32_t gv) {
                if (globalToLocal.find(gv) == globalToLocal.end()) {
                    uint32_t lv = static_cast<uint32_t>(globalToLocal.size());
                    globalToLocal[gv] = lv;
                    meshlet.vertexIndices.push_back(gv);
                }
            };
            addVert(t.v0); addVert(t.v1); addVert(t.v2);

            meshlet.triangleIndices.push_back(globalToLocal[t.v0]);
            meshlet.triangleIndices.push_back(globalToLocal[t.v1]);
            meshlet.triangleIndices.push_back(globalToLocal[t.v2]);
            meshletTris.push_back(seed);
            assigned[seed] = true;
        }

        // Greedily add adjacent triangles
        bool added = true;
        while (added) {
            added = false;

            for (size_t mi = 0; mi < meshletTris.size(); ++mi) {
                size_t ti = meshletTris[mi];
                for (size_t nj : triAdj[ti]) {
                    if (assigned[nj]) continue;

                    const auto& t = triFaces[nj];

                    // Check vertex budget
                    uint32_t newVertsNeeded = static_cast<uint32_t>(globalToLocal.size());
                    std::vector<uint32_t> newVertCandidates;
                    for (uint32_t v : {t.v0, t.v1, t.v2}) {
                        if (globalToLocal.find(v) == globalToLocal.end()) {
                            newVertCandidates.push_back(v);
                        }
                    }
                    if (newVertsNeeded + newVertCandidates.size() > static_cast<size_t>(maxVerts))
                        continue;

                    // Check triangle budget
                    if (meshletTris.size() + 1 > static_cast<size_t>(maxTris)) continue;

                    // Add it
                    for (uint32_t gv : newVertCandidates) {
                        uint32_t lv = static_cast<uint32_t>(globalToLocal.size());
                        globalToLocal[gv] = lv;
                        meshlet.vertexIndices.push_back(gv);
                    }

                    meshlet.triangleIndices.push_back(globalToLocal[t.v0]);
                    meshlet.triangleIndices.push_back(globalToLocal[t.v1]);
                    meshlet.triangleIndices.push_back(globalToLocal[t.v2]);
                    meshletTris.push_back(nj);
                    assigned[nj] = true;
                    added = true;
                }
            }
        }

        meshlet.vertexCount = static_cast<uint32_t>(meshlet.vertexIndices.size());
        meshlet.triangleCount = static_cast<uint32_t>(meshletTris.size());

        // --- Compute AABB and normal cone ------------------------------------
        {
            const auto& positions = mesh.attributes().positions();

            nexus::render::Vec3 aabbMin = {std::numeric_limits<float>::max(),
                                           std::numeric_limits<float>::max(),
                                           std::numeric_limits<float>::max()};
            nexus::render::Vec3 aabbMax = {-std::numeric_limits<float>::max(),
                                           -std::numeric_limits<float>::max(),
                                           -std::numeric_limits<float>::max()};

            nexus::render::Vec3 avgNormal = {0.f, 0.f, 0.f};
            std::vector<nexus::render::Vec3> faceNormals;
            faceNormals.reserve(meshletTris.size());

            for (size_t ti : meshletTris) {
                const auto& t = triFaces[ti];
                const auto& p0 = positions[t.v0];
                const auto& p1 = positions[t.v1];
                const auto& p2 = positions[t.v2];

                aabbMin.x = std::min({aabbMin.x, p0.x, p1.x, p2.x});
                aabbMin.y = std::min({aabbMin.y, p0.y, p1.y, p2.y});
                aabbMin.z = std::min({aabbMin.z, p0.z, p1.z, p2.z});
                aabbMax.x = std::max({aabbMax.x, p0.x, p1.x, p2.x});
                aabbMax.y = std::max({aabbMax.y, p0.y, p1.y, p2.y});
                aabbMax.z = std::max({aabbMax.z, p0.z, p1.z, p2.z});

                nexus::render::Vec3 e1 = p1 - p0;
                nexus::render::Vec3 e2 = p2 - p0;
                nexus::render::Vec3 n = e1.cross(e2).normalize();
                faceNormals.push_back(n);
                avgNormal = avgNormal + n;
            }

            meshlet.aabb = {aabbMin, aabbMax};

            if (!faceNormals.empty()) {
                avgNormal = avgNormal.normalize();

                float minDot = 1.f;
                for (const auto& n : faceNormals) {
                    float d = avgNormal.dot(n);
                    if (d < minDot) minDot = d;
                }

                meshlet.normalCone.axis       = avgNormal;
                meshlet.normalCone.angleCutoff = minDot;
                meshlet.normalCone.apexOffset = meshlet.aabb.center();
            }
        }

        // --- Build primitive indices (for mesh shader output) -----------------
        // Each triangle encodes as 3 indices for pre-transform vertex reuse
        for (uint32_t i = 0; i < meshlet.triangleCount; ++i) {
            meshlet.primitiveIndices.push_back(0); // indexCount per primitive (0 = end)
        }
        // Primitive indices: for each triangle, encode (3, 2*3) as:
        // 0 | 3, 0 | 3, ...
        // Actually: encode index offset and count
        // For simplicity: each primitive has 3 indices.
        // The encoding: for first triangle with 3 indices: 3 (no offset needed with 0-indexed prim)
        // Actually mesh shader convention is {3, 3, 3...} for triangles.
        // Let me use a standard encoding.
        meshlet.primitiveIndices.clear();
        for (uint32_t i = 0; i < meshlet.triangleCount; ++i) {
            meshlet.primitiveIndices.push_back(3); // 3 indices per primitive
        }

        result.meshlets.push_back(std::move(meshlet));
    }

    result.ok = true;
    return result;
}

} // namespace nexus::geometry
