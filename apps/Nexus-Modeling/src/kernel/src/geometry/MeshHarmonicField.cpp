#include <nexus/geometry/MeshHarmonicField.h>

#include <cmath>
#include <map>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

float cotan(const Vec3& a, const Vec3& b) {
    float dotAB = a.dot(b);
    float crossLen = a.cross(b).length();
    if (crossLen < 1e-10f) return 0.f;
    return dotAB / crossLen;
}

} // namespace

std::vector<float> MeshHarmonicField::solve(const Mesh& mesh,
                                              const HarmonicConstraints& constraints,
                                              const HarmonicOptions& opts) {
    if (!mesh.isValid()) return {};

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();
    if (V == 0) return {};

    std::map<uint64_t, float> cotWeights;
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() < 3) continue;
        for (size_t vi = 0; vi < f.indices.size(); ++vi) {
            uint32_t v0 = f.indices[vi];
            uint32_t v1 = f.indices[(vi + 1) % f.indices.size()];
            uint32_t v2 = f.indices[(vi + 2) % f.indices.size()];
            Vec3 e1 = pos[v1] - pos[v0];
            Vec3 e2 = pos[v2] - pos[v0];
            float cot0 = cotan(e1 * -1.f, e2 * -1.f);
            uint64_t key12 = (static_cast<uint64_t>(std::min(v1, v2)) << 32) | std::max(v1, v2);
            cotWeights[key12] += cot0;
        }
    }

    std::vector<float> phi(V, 0.f);
    std::unordered_map<uint32_t, float> dirichlet;
    std::vector<bool> constrained(V, false);

    if (constraints.constrainedVertices.size() != constraints.constrainedValues.size())
        return {};

    for (size_t i = 0; i < constraints.constrainedVertices.size(); ++i) {
        uint32_t vi = constraints.constrainedVertices[i];
        if (vi < V) {
            constrained[vi] = true;
            phi[vi] = constraints.constrainedValues[i];
            dirichlet[vi] = constraints.constrainedValues[i];
        }
    }

    std::vector<std::vector<std::pair<uint32_t, float>>> adjacency(V);
    std::vector<float> rowSum(V, 0.f);
    for (const auto& [key, w] : cotWeights) {
        if (std::fabs(w) < 1e-12f) continue;
        uint32_t a = static_cast<uint32_t>(key >> 32);
        uint32_t b = static_cast<uint32_t>(key & 0xFFFFFFFF);
        adjacency[a].emplace_back(b, w);
        adjacency[b].emplace_back(a, w);
        rowSum[a] += w;
        rowSum[b] += w;
    }

    for (uint32_t iter = 0; iter < opts.maxIterations; ++iter) {
        float maxChange = 0.f;
        for (size_t i = 0; i < V; ++i) {
            if (constrained[i]) continue;
            if (rowSum[i] < 1e-10f) continue;

            float sum = 0.f;
            for (const auto& [j, w] : adjacency[i]) {
                sum += w * phi[j];
            }

            float newPhi = (1.f - opts.omega) * phi[i] + opts.omega * (sum / rowSum[i]);
            float change = std::fabs(newPhi - phi[i]);
            if (change > maxChange) maxChange = change;
            phi[i] = newPhi;
        }
        if (maxChange < opts.tolerance && iter > 2) break;
    }

    return phi;
}

} // namespace nexus::geometry
