#include <nexus/geometry/GeodesicPath.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

static const int32_t kDirs[8][2] = {
    {-1, -1}, {-1, 0}, {-1, 1},
    { 0, -1},          { 0, 1},
    { 1, -1}, { 1, 0}, { 1, 1}
};

static const float kDiagWeight = 1.4142135623730951f;

GeodesicPath::Result GeodesicPath::compute(const NurbsSurface& surface,
                                            float u0, float v0,
                                            float u1, float v1,
                                            int32_t gridRes) {
    Result result;
    if (gridRes < 2) return result;

    auto domU = surface.domainU();
    auto domV = surface.domainV();

    // Compute 3D positions for each grid vertex + edge weights
    std::vector<std::vector<Vec3>> gridPos(
        static_cast<size_t>(gridRes),
        std::vector<Vec3>(static_cast<size_t>(gridRes)));

    for (int32_t i = 0; i < gridRes; ++i) {
        float u = domU.first + (domU.second - domU.first) * static_cast<float>(i) / static_cast<float>(gridRes - 1);
        for (int32_t j = 0; j < gridRes; ++j) {
            float v = domV.first + (domV.second - domV.first) * static_cast<float>(j) / static_cast<float>(gridRes - 1);
            gridPos[static_cast<size_t>(i)][static_cast<size_t>(j)] = surface.evaluate(u, v);
        }
    }

    auto edgeLen = [&](int32_t i, int32_t j, int32_t ni, int32_t nj) -> float {
        Vec3 a = gridPos[static_cast<size_t>(i)][static_cast<size_t>(j)];
        Vec3 b = gridPos[static_cast<size_t>(ni)][static_cast<size_t>(nj)];
        return (b - a).length();
    };

    int32_t startI = static_cast<int32_t>(std::clamp(u0 / (domU.second - domU.first), 0.f, 1.f) * (gridRes - 1) + 0.5f);
    int32_t startJ = static_cast<int32_t>(std::clamp(v0 / (domV.second - domV.first), 0.f, 1.f) * (gridRes - 1) + 0.5f);
    int32_t endI   = static_cast<int32_t>(std::clamp(u1 / (domU.second - domU.first), 0.f, 1.f) * (gridRes - 1) + 0.5f);
    int32_t endJ   = static_cast<int32_t>(std::clamp(v1 / (domV.second - domV.first), 0.f, 1.f) * (gridRes - 1) + 0.5f);

    startI = std::clamp(startI, 0, gridRes - 1);
    startJ = std::clamp(startJ, 0, gridRes - 1);
    endI   = std::clamp(endI, 0, gridRes - 1);
    endJ   = std::clamp(endJ, 0, gridRes - 1);

    size_t totalCells = static_cast<size_t>(gridRes * gridRes);
    std::vector<float> dist(totalCells, std::numeric_limits<float>::infinity());
    std::vector<int32_t> prev(totalCells, -1);

    using PQItem = std::pair<float, int32_t>;
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> pq;

    auto cellIdx = [&](int32_t i, int32_t j) -> int32_t {
        return i * gridRes + j;
    };

    size_t startCell = static_cast<size_t>(cellIdx(startI, startJ));
    dist[startCell] = 0.f;
    pq.push({0.f, static_cast<int32_t>(startCell)});

    while (!pq.empty()) {
        auto [d, cell] = pq.top();
        pq.pop();

        if (static_cast<float>(d) > dist[static_cast<size_t>(cell)]) continue;

        int32_t ci = cell / gridRes;
        int32_t cj = cell % gridRes;

        for (int dk = 0; dk < 8; ++dk) {
            int32_t ni = ci + kDirs[dk][0];
            int32_t nj = cj + kDirs[dk][1];
            if (ni < 0 || ni >= gridRes || nj < 0 || nj >= gridRes) continue;

            float w = edgeLen(ci, cj, ni, nj);
            size_t nidx = static_cast<size_t>(cellIdx(ni, nj));
            float nd = d + w;

            if (nd < dist[nidx]) {
                dist[nidx] = nd;
                prev[nidx] = cell;
                pq.push({nd, static_cast<int32_t>(nidx)});
            }
        }
    }

    size_t endCell = static_cast<size_t>(cellIdx(endI, endJ));
    if ((std::bit_cast<uint32_t>(dist[endCell]) & 0x7F800000u) == 0x7F800000u) return result;

    std::vector<int32_t> path;
    int32_t cur = static_cast<int32_t>(endCell);
    while (cur != -1) {
        path.push_back(cur);
        if (static_cast<size_t>(cur) == startCell) break;
        cur = prev[static_cast<size_t>(cur)];
    }
    std::reverse(path.begin(), path.end());

    for (int32_t idx : path) {
        int32_t ci = idx / gridRes;
        int32_t cj = idx % gridRes;
        result.path3D.push_back(gridPos[static_cast<size_t>(ci)][static_cast<size_t>(cj)]);
    }

    result.arcLength = dist[endCell];
    return result;
}

} // namespace nexus::geometry
