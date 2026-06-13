#include <nexus/geometry/SurfacePatchNetwork.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

void SurfacePatchNetwork::addPatch(const NurbsSurface& surface) {
    m_patches.push_back(surface);
}

void SurfacePatchNetwork::connect(int32_t patchA, int32_t edgeA,
                                   int32_t patchB, int32_t edgeB) {
    Connection conn;
    conn.a.patch = patchA;
    conn.a.edge  = edgeA;
    conn.b.patch = patchB;
    conn.b.edge  = edgeB;
    m_connections.push_back(conn);
}

std::vector<SurfacePatchNetwork::EdgeId> SurfacePatchNetwork::neighbors(int32_t patch) const {
    std::vector<EdgeId> result;
    for (const auto& c : m_connections) {
        if (c.a.patch == patch) result.push_back(c.b);
        else if (c.b.patch == patch) result.push_back(c.a);
    }
    return result;
}

Mesh SurfacePatchNetwork::tessellate(int32_t samplesU, int32_t samplesV) const {
    Mesh result;
    auto& attrs = result.attributes();
    auto& topo  = result.topology();

    if (m_patches.empty() || samplesU < 2 || samplesV < 2) return result;

    size_t totalPatches = m_patches.size();
    std::vector<Vec3> positions;
    std::vector<std::vector<uint32_t>> patchIndices;

    for (size_t p = 0; p < totalPatches; ++p) {
        const auto& surf = m_patches[p];
        auto domU = surf.domainU();
        auto domV = surf.domainV();
        float du = (domU.second - domU.first) / static_cast<float>(samplesU - 1);
        float dv = (domV.second - domV.first) / static_cast<float>(samplesV - 1);

        uint32_t baseIdx = static_cast<uint32_t>(positions.size());
        std::vector<uint32_t> grid;
        grid.resize(static_cast<size_t>(samplesU * samplesV));

        for (int32_t i = 0; i < samplesU; ++i) {
            float u = domU.first + du * static_cast<float>(i);
            for (int32_t j = 0; j < samplesV; ++j) {
                float v = domV.first + dv * static_cast<float>(j);
                positions.push_back(surf.evaluate(u, v));
                grid[static_cast<size_t>(i * samplesV + j)] =
                    baseIdx + static_cast<uint32_t>(i * samplesV + j);
            }
        }

        for (int32_t i = 0; i < samplesU - 1; ++i) {
            for (int32_t j = 0; j < samplesV - 1; ++j) {
                uint32_t a = grid[static_cast<size_t>(i * samplesV + j)];
                uint32_t b = grid[static_cast<size_t>(i * samplesV + j + 1)];
                uint32_t c = grid[static_cast<size_t>((i + 1) * samplesV + j + 1)];
                uint32_t d = grid[static_cast<size_t>((i + 1) * samplesV + j)];
                Face f1; f1.indices = {a, b, c}; topo.addFace(f1);
                Face f2; f2.indices = {a, c, d}; topo.addFace(f2);
            }
        }

        patchIndices.push_back(std::move(grid));
    }

    if (!positions.empty()) {
        attrs.setPositions(positions);
    }

    return result;
}

} // namespace nexus::geometry
