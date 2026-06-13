#include <nexus/geometry/PointCloudNormals.h>
#include "EigenSolver3x3.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

struct CellKey {
    int32_t ix, iy, iz;
    bool operator==(const CellKey& o) const noexcept {
        return ix == o.ix && iy == o.iy && iz == o.iz;
    }
};

struct CellKeyHash {
    size_t operator()(const CellKey& k) const noexcept {
        size_t h = static_cast<size_t>(k.ix) * 73856093;
        h ^= static_cast<size_t>(k.iy) * 19349663;
        h ^= static_cast<size_t>(k.iz) * 83492791;
        return h;
    }
};

using SpatialGrid = std::unordered_map<CellKey, std::vector<uint32_t>, CellKeyHash>;

float estimateAverageNearestNeighborDist(const std::vector<Vec3>& points) {
    size_t n = points.size();
    if (n < 3) return 1.0f;

    const size_t sampleCount = std::min<size_t>(500, n);
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;

    std::mt19937 rng(42);
    std::shuffle(indices.begin(), indices.end(), rng);

    double sumDist = 0.0;
    for (size_t si = 0; si < sampleCount; ++si) {
        size_t idx = indices[si];
        const Vec3& pi = points[idx];
        float bestD = std::numeric_limits<float>::max();
        for (size_t j = 0; j < n; ++j) {
            if (j == idx) continue;
            float d = (points[j] - pi).lengthSq();
            if (d < bestD) bestD = d;
        }
        sumDist += std::sqrt(static_cast<double>(bestD));
    }

    return static_cast<float>(sumDist / static_cast<double>(sampleCount));
}

} // anonymous namespace

PointCloudNormals::Result PointCloudNormals::estimate(
    const std::vector<Vec3>& points, const PointCloudNormalsOptions& opts) {

    Result result;
    size_t n = points.size();
    if (n < 3) return result;

    int32_t k = std::min(opts.kNeighbors, static_cast<int32_t>(n) - 1);
    if (k < 3) k = static_cast<int32_t>(std::min(n - 1, size_t{3}));

    result.normals.resize(n);

    float avgDist = estimateAverageNearestNeighborDist(points);
    float cellSize = std::max(avgDist * 3.0f, 1e-4f);
    float invCellSize = 1.0f / cellSize;

    SpatialGrid grid;
    grid.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        int32_t ix = static_cast<int32_t>(std::floor(points[i].x * invCellSize));
        int32_t iy = static_cast<int32_t>(std::floor(points[i].y * invCellSize));
        int32_t iz = static_cast<int32_t>(std::floor(points[i].z * invCellSize));
        grid[{ix, iy, iz}].push_back(static_cast<uint32_t>(i));
    }

    std::vector<uint32_t> cellNeighbors;
    std::vector<std::pair<float, uint32_t>> dists;
    cellNeighbors.reserve(static_cast<size_t>(k) * 64);
    dists.reserve(static_cast<size_t>(k) * 64);

    for (size_t i = 0; i < n; ++i) {
        const Vec3& pi = points[i];

        int32_t cix = static_cast<int32_t>(std::floor(pi.x * invCellSize));
        int32_t ciy = static_cast<int32_t>(std::floor(pi.y * invCellSize));
        int32_t ciz = static_cast<int32_t>(std::floor(pi.z * invCellSize));

        cellNeighbors.clear();
        for (int32_t dx = -1; dx <= 1; ++dx) {
            for (int32_t dy = -1; dy <= 1; ++dy) {
                for (int32_t dz = -1; dz <= 1; ++dz) {
                    auto it = grid.find({cix + dx, ciy + dy, ciz + dz});
                    if (it == grid.end()) continue;
                    for (uint32_t j : it->second) {
                        if (j != static_cast<uint32_t>(i)) {
                            cellNeighbors.push_back(j);
                        }
                    }
                }
            }
        }

        dists.clear();
        if (cellNeighbors.size() < static_cast<size_t>(k)) {
            for (size_t j = 0; j < n; ++j) {
                if (j == i) continue;
                float d = (points[j] - pi).lengthSq();
                dists.emplace_back(d, static_cast<uint32_t>(j));
            }
        } else {
            for (uint32_t j : cellNeighbors) {
                float d = (points[j] - pi).lengthSq();
                dists.emplace_back(d, j);
            }
        }

        std::sort(dists.begin(), dists.end());
        size_t kn = std::min(dists.size(), static_cast<size_t>(k));

        Vec3 centroid{0.f, 0.f, 0.f};
        for (size_t j = 0; j < kn; ++j) {
            centroid += points[dists[j].second];
        }
        centroid = centroid * (1.0f / static_cast<float>(kn));

        float m[6] = {};
        for (size_t j = 0; j < kn; ++j) {
            Vec3 dv = points[dists[j].second] - centroid;
            m[0] += dv.x * dv.x;
            m[1] += dv.y * dv.y;
            m[2] += dv.z * dv.z;
            m[3] += dv.x * dv.y;
            m[4] += dv.x * dv.z;
            m[5] += dv.y * dv.z;
        }

        nexus::geometry::internal::Eigen3x3 eig =
            nexus::geometry::internal::solveEigenSymmetric3x3(m);

        int minIdx = 0;
        float minVal = std::abs(eig.val[0]);
        for (int j = 1; j < 3; ++j) {
            if (std::abs(eig.val[j]) < minVal) {
                minVal = std::abs(eig.val[j]);
                minIdx = j;
            }
        }

        Vec3 normal = eig.vec[minIdx];
        float len = normal.length();
        if (len > 1e-8f) normal = normal * (1.0f / len);

        if (opts.orientToViewpoint) {
            Vec3 toView = opts.viewpoint - pi;
            if (normal.dot(toView) < 0.f) normal = Vec3{-normal.x, -normal.y, -normal.z};
        }

        result.normals[i] = normal;
    }

    return result;
}

} // namespace nexus::geometry
