#include <nexus/geometry/MeshCageDeform.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh MeshCageDeform::bind(const Mesh& mesh, const FFDGrid& grid) {
    Mesh result = mesh;

    if (!result.isValid() || grid.controlPoints.empty()) {
        m_bindingIndices.clear();
        m_bindingWeights.clear();
        return result;
    }

    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();
    uint32_t L = grid.resolution[0];
    uint32_t M = grid.resolution[1];
    uint32_t N = grid.resolution[2];

    float sx = grid.cellSize.x;
    float sy = grid.cellSize.y;
    float sz = grid.cellSize.z;

    m_bindingIndices.resize(V);
    m_bindingWeights.resize(V);

    auto flatIdx = [L, M](uint32_t i, uint32_t j, uint32_t k) -> uint32_t {
        return i + j * L + k * L * M;
    };

    for (size_t vi = 0; vi < V; ++vi) {
        Vec3 P = pos[vi];
        Vec3 local = {
            (P.x - grid.origin.x) / sx,
            (P.y - grid.origin.y) / sy,
            (P.z - grid.origin.z) / sz,
        };

        int ci = std::max(0, std::min(static_cast<int>(L) - 2, static_cast<int>(std::floor(local.x))));
        int cj = std::max(0, std::min(static_cast<int>(M) - 2, static_cast<int>(std::floor(local.y))));
        int ck = std::max(0, std::min(static_cast<int>(N) - 2, static_cast<int>(std::floor(local.z))));

        float s = std::max(0.f, std::min(1.f, local.x - static_cast<float>(ci)));
        float t = std::max(0.f, std::min(1.f, local.y - static_cast<float>(cj)));
        float u = std::max(0.f, std::min(1.f, local.z - static_cast<float>(ck)));

        m_bindingIndices[vi] = {
            flatIdx(ci,     cj,     ck),
            flatIdx(ci + 1, cj,     ck),
            flatIdx(ci,     cj + 1, ck),
            flatIdx(ci,     cj,     ck + 1),
            flatIdx(ci + 1, cj + 1, ck),
            flatIdx(ci + 1, cj,     ck + 1),
            flatIdx(ci,     cj + 1, ck + 1),
            flatIdx(ci + 1, cj + 1, ck + 1),
        };

        m_bindingWeights[vi] = {
            (1.f - s) * (1.f - t) * (1.f - u),
            s         * (1.f - t) * (1.f - u),
            (1.f - s) * t         * (1.f - u),
            (1.f - s) * (1.f - t) * u,
            s         * t         * (1.f - u),
            s         * (1.f - t) * u,
            (1.f - s) * t         * u,
            s         * t         * u,
        };
    }

    return result;
}

Mesh MeshCageDeform::deform(const Mesh& mesh,
                             const std::vector<Vec3>& deformedCagePoints) {
    Mesh result = mesh;
    if (!result.isValid() || deformedCagePoints.empty()) return result;

    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();

    if (m_bindingIndices.empty() || m_bindingIndices.size() != V) return result;
    if (m_bindingWeights.size() != V) return result;

    std::vector<Vec3> newPos;
    newPos.reserve(V);

    for (size_t vi = 0; vi < V; ++vi) {
        Vec3 P = {0, 0, 0};
        for (int ci = 0; ci < 8; ++ci) {
            uint32_t cpi = m_bindingIndices[vi][ci];
            float w = m_bindingWeights[vi][ci];
            if (cpi < deformedCagePoints.size()) {
                P = P + deformedCagePoints[cpi] * w;
            } else {
                P = P + pos[vi] * w;
            }
        }
        newPos.push_back(P);
    }

    result.attributes().setPositions(std::move(newPos));
    return result;
}

} // namespace nexus::geometry
