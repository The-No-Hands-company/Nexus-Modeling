#include <nexus/geometry/MeshIslandDecomposition.h>

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

uint64_t edgeKey(uint32_t a, uint32_t b) {
    if (a < b) return (static_cast<uint64_t>(a) << 32) | b;
    return (static_cast<uint64_t>(b) << 32) | a;
}

} // namespace

std::vector<Mesh> MeshIslandDecomposition::decompose(const Mesh& mesh) {
    std::vector<Mesh> islands;
    if (!mesh.isValid()) return islands;

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    size_t faceCount = topo.faceCount();
    if (faceCount == 0) return islands;

    std::map<uint64_t, std::vector<size_t>> edgeToFaces;
    for (size_t fi = 0; fi < faceCount; ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() < 3) continue;
        for (size_t vi = 0; vi < f.indices.size(); ++vi) {
            uint32_t v0 = f.indices[vi];
            uint32_t v1 = f.indices[(vi + 1) % f.indices.size()];
            edgeToFaces[edgeKey(v0, v1)].push_back(fi);
        }
    }

    std::vector<std::vector<size_t>> adjacency(faceCount);
    for (auto& [ek, faces] : edgeToFaces) {
        if (faces.size() > 1) {
            for (size_t i = 0; i < faces.size(); ++i)
                for (size_t j = i + 1; j < faces.size(); ++j) {
                    adjacency[faces[i]].push_back(faces[j]);
                    adjacency[faces[j]].push_back(faces[i]);
                }
        }
    }

    std::vector<int8_t> visited(faceCount, 0);
    const auto& norms = mesh.attributes().normals();
    const auto& uvs = mesh.attributes().uvs();
    const auto& tangents = mesh.attributes().tangents();

    for (size_t fi = 0; fi < faceCount; ++fi) {
        if (visited[fi]) continue;

        std::vector<size_t> component;
        std::queue<size_t> q;
        q.push(fi);
        visited[fi] = 1;

        while (!q.empty()) {
            size_t current = q.front();
            q.pop();
            component.push_back(current);
            for (size_t neighbor : adjacency[current]) {
                if (!visited[neighbor]) {
                    visited[neighbor] = 1;
                    q.push(neighbor);
                }
            }
        }

        std::unordered_map<uint32_t, uint32_t> vRemap;
        std::vector<Vec3> newPos;
        std::vector<Vec3> newNorms;
        std::vector<Vec2> newUVs;
        std::vector<Vec4> newTangents;
        Mesh island;

        for (size_t cfi : component) {
            const auto& face = topo.face(cfi);
            Face newFace;
            for (uint32_t vi : face.indices) {
                if (vRemap.find(vi) == vRemap.end()) {
                    vRemap[vi] = static_cast<uint32_t>(newPos.size());
                    newPos.push_back(pos[vi]);
                    if (!norms.empty()) newNorms.push_back(norms[vi]);
                    if (!uvs.empty()) newUVs.push_back(uvs[vi]);
                    if (!tangents.empty()) newTangents.push_back(tangents[vi]);
                }
                newFace.indices.push_back(vRemap[vi]);
            }
            island.topology().addFace(std::move(newFace));
        }

        island.attributes().setPositions(std::move(newPos));
        if (!norms.empty()) island.attributes().setNormals(std::move(newNorms));
        if (!uvs.empty()) island.attributes().setUVs(std::move(newUVs));
        if (!tangents.empty()) island.attributes().setTangents(std::move(newTangents));

        islands.push_back(std::move(island));
    }

    return islands;
}

size_t MeshIslandDecomposition::count(const Mesh& mesh) {
    if (!mesh.isValid()) return 0;
    return decompose(mesh).size();
}

} // namespace nexus::geometry
