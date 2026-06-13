#include <nexus/geometry/MeshGeodesicDistance.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_set>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

void buildEdgeGraph(const Mesh& mesh,
                     std::vector<std::vector<std::pair<uint32_t, float>>>& adjacency) {
    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();

    adjacency.assign(V, {});

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() < 2) continue;
        for (size_t vi = 0; vi < f.indices.size(); ++vi) {
            uint32_t u = f.indices[vi];
            uint32_t v = f.indices[(vi + 1) % f.indices.size()];
            float dist = (pos[u] - pos[v]).length();

            bool found = false;
            for (auto& edge : adjacency[u]) {
                if (edge.first == v) {
                    edge.second = std::min(edge.second, dist);
                    found = true;
                    break;
                }
            }
            if (!found) adjacency[u].emplace_back(v, dist);

            found = false;
            for (auto& edge : adjacency[v]) {
                if (edge.first == u) {
                    edge.second = std::min(edge.second, dist);
                    found = true;
                    break;
                }
            }
            if (!found) adjacency[v].emplace_back(u, dist);
        }
    }
}

void propagateComponents(const std::vector<std::vector<std::pair<uint32_t, float>>>& adjacency,
                          std::vector<int32_t>& componentIds, uint32_t start, int32_t compId) {
    std::vector<uint32_t> stack;
    stack.push_back(start);
    componentIds[start] = compId;

    while (!stack.empty()) {
        uint32_t u = stack.back();
        stack.pop_back();
        for (const auto& [v, w] : adjacency[u]) {
            if (componentIds[v] < 0) {
                componentIds[v] = compId;
                stack.push_back(v);
            }
        }
    }
}

} // namespace

GeodesicResult MeshGeodesicDistance::fromVertex(const Mesh& mesh, uint32_t sourceVertex) {
    return fromVertices(mesh, {sourceVertex});
}

GeodesicResult MeshGeodesicDistance::fromVertices(const Mesh& mesh,
                                                    const std::vector<uint32_t>& sources) {
    GeodesicResult result;
    if (!mesh.isValid() || sources.empty()) return result;

    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();
    if (V == 0) return result;

    std::vector<std::vector<std::pair<uint32_t, float>>> adjacency;
    buildEdgeGraph(mesh, adjacency);

    result.distances.assign(V, std::numeric_limits<float>::infinity());
    result.predecessors.assign(V, -1);

    std::vector<int32_t> componentIds(V, -1);
    int32_t nextCompId = 0;
    for (size_t i = 0; i < V; ++i) {
        if (componentIds[i] < 0) {
            propagateComponents(adjacency, componentIds, static_cast<uint32_t>(i), nextCompId++);
        }
    }

    std::vector<uint32_t> validSources;
    for (uint32_t s : sources) {
        if (s < V && componentIds[s] >= 0) {
            validSources.push_back(s);
        }
    }
    if (validSources.empty()) return result;

    using Entry = std::pair<float, uint32_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;

    std::unordered_set<uint32_t> sourceSet;
    for (uint32_t src : validSources) {
        if (sourceSet.insert(src).second) {
            result.distances[src] = 0.f;
            result.predecessors[src] = static_cast<int32_t>(src);
            pq.emplace(0.f, src);
        }
    }

    while (!pq.empty()) {
        auto [dist, u] = pq.top();
        pq.pop();
        if (dist > result.distances[u]) continue;

        for (const auto& [v, weight] : adjacency[u]) {
            float newDist = dist + weight;
            if (newDist < result.distances[v]) {
                result.distances[v] = newDist;
                result.predecessors[v] = static_cast<int32_t>(u);
                pq.emplace(newDist, v);
            }
        }
    }

    return result;
}

} // namespace nexus::geometry
