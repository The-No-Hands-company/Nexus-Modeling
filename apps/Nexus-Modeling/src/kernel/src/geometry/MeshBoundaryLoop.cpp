#include <nexus/geometry/MeshBoundaryLoop.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

BoundaryLoopResult MeshBoundaryLoop::extract(const Mesh& mesh) {
    BoundaryLoopResult result;
    if (!mesh.isValid()) return result;

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();

    std::unordered_map<uint64_t, int> halfEdgeCount;

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() < 3) continue;
        for (size_t vi = 0; vi < f.indices.size(); ++vi) {
            uint32_t v0 = f.indices[vi];
            uint32_t v1 = f.indices[(vi + 1) % f.indices.size()];
            uint64_t key = (static_cast<uint64_t>(v0) << 32) | v1;
            halfEdgeCount[key]++;
        }
    }

    // A directed half-edge (a→b) is a boundary if it appears exactly once
    // and its opposite (b→a) does not appear at all.
    std::unordered_map<uint32_t, std::vector<uint32_t>> boundaryAdj;
    for (const auto& [key, count] : halfEdgeCount) {
        if (count != 1) continue;
        uint32_t from = static_cast<uint32_t>(key >> 32);
        uint32_t to = static_cast<uint32_t>(key & 0xFFFFFFFF);
        uint64_t oppositeKey = (static_cast<uint64_t>(to) << 32) | from;
        if (halfEdgeCount.find(oppositeKey) != halfEdgeCount.end()) continue;
        boundaryAdj[from].push_back(to);
    }

    std::unordered_set<uint32_t> visited;
    for (auto& [start, neighbors] : boundaryAdj) {
        if (visited.count(start)) continue;

        std::vector<uint32_t> loop;
        float perimeter = 0.f;
        uint32_t current = start;

        while (!visited.count(current)) {
            loop.push_back(current);
            visited.insert(current);

            auto it = boundaryAdj.find(current);
            if (it == boundaryAdj.end()) break;

            uint32_t next = std::numeric_limits<uint32_t>::max();
            for (uint32_t cand : it->second) {
                if (!visited.count(cand)) {
                    next = cand;
                    break;
                }
            }
            if (next == std::numeric_limits<uint32_t>::max()) break;

            perimeter += (pos[current] - pos[next]).length();
            current = next;
        }

        if (loop.size() > 2) {
            perimeter += (pos[current] - pos[loop[0]]).length();
            result.loops.push_back(std::move(loop));
            result.perimeters.push_back(perimeter);
        }
    }

    return result;
}

} // namespace nexus::geometry
