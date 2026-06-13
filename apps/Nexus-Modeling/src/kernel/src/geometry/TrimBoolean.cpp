#include <nexus/geometry/TrimBoolean.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;
using Aabb = nexus::render::Aabb;

static Aabb regionBounds(const BooleanRegion& region) {
    Aabb box;
    box.min = Vec3{std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()};
    box.max = Vec3{-std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max()};

    for (const auto& loop : region.outerLoops) {
        for (const auto& pt : loop.points) {
            box.min.x = std::min(box.min.x, pt.x);
            box.min.y = std::min(box.min.y, pt.y);
            box.max.x = std::max(box.max.x, pt.x);
            box.max.y = std::max(box.max.y, pt.y);
        }
    }
    for (const auto& loop : region.innerLoops) {
        for (const auto& pt : loop.points) {
            box.min.x = std::min(box.min.x, pt.x);
            box.min.y = std::min(box.min.y, pt.y);
            box.max.x = std::max(box.max.x, pt.x);
            box.max.y = std::max(box.max.y, pt.y);
        }
    }
    return box;
}

static bool pointInLoop(const Vec3& pt, const BooleanLoop& loop) {
    int32_t n = static_cast<int32_t>(loop.points.size());
    if (n < 3) return false;

    bool inside = false;
    for (int32_t i = 0, j = n - 1; i < n; j = i++) {
        const Vec3& pi = loop.points[static_cast<size_t>(i)];
        const Vec3& pj = loop.points[static_cast<size_t>(j)];

        if (((pi.y > pt.y) != (pj.y > pt.y)) &&
            (pt.x < (pj.x - pi.x) * (pt.y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
    }
    return inside;
}

static bool pointInRegion(const Vec3& pt, const BooleanRegion& region) {
    bool inOuter = false;
    for (const auto& loop : region.outerLoops) {
        if (pointInLoop(pt, loop)) {
            inOuter = true;
            break;
        }
    }
    if (!inOuter) return false;

    for (const auto& loop : region.innerLoops) {
        if (pointInLoop(pt, loop)) return false;
    }
    return true;
}

static void rasterizeToGrid(const BooleanRegion& region,
                             const Aabb& bounds,
                             int32_t res,
                             std::vector<uint8_t>& mask) {
    mask.assign(static_cast<size_t>(res * res), 0);
    if (region.empty()) return;

    Vec3 extent = bounds.extents();
    Vec3 center = bounds.center();

    for (int32_t j = 0; j < res; ++j) {
        for (int32_t i = 0; i < res; ++i) {
            float u = (static_cast<float>(i) / static_cast<float>(res - 1) - 0.5f) * 2.0f;
            float v = (static_cast<float>(j) / static_cast<float>(res - 1) - 0.5f) * 2.0f;
            Vec3 pt{center.x + u * extent.x,
                     center.y + v * extent.y,
                     0.f};
            if (pointInRegion(pt, region)) {
                mask[static_cast<size_t>(j * res + i)] = 1;
            }
        }
    }
}

static BooleanLoop extractBoundary(const std::vector<uint8_t>& mask,
                                     int32_t res,
                                     const Aabb& bounds) {
    BooleanLoop boundary;

    Vec3 extent = bounds.extents();
    Vec3 center = bounds.center();

    auto gridToWorld = [&](int32_t i, int32_t j) -> Vec3 {
        float u = (static_cast<float>(i) / static_cast<float>(res - 1) - 0.5f) * 2.0f;
        float v = (static_cast<float>(j) / static_cast<float>(res - 1) - 0.5f) * 2.0f;
        return Vec3{center.x + u * extent.x,
                     center.y + v * extent.y,
                     0.f};
    };

    auto isBorder = [&](int32_t i, int32_t j) -> bool {
        if (!mask[static_cast<size_t>(j * res + i)]) return false;
        if (i <= 0 || i >= res - 1 || j <= 0 || j >= res - 1) return true;
        return !mask[static_cast<size_t>(j * res + i + 1)] ||
               !mask[static_cast<size_t>(j * res + i - 1)] ||
               !mask[static_cast<size_t>((j + 1) * res + i)] ||
               !mask[static_cast<size_t>((j - 1) * res + i)];
    };

    for (int32_t j = 0; j < res; ++j) {
        int32_t edgeStart = -1;
        for (int32_t i = 0; i < res; ++i) {
            if (isBorder(i, j)) {
                if (edgeStart < 0) edgeStart = i;
            } else {
                if (edgeStart >= 0 && (i - edgeStart) > 1) {
                    boundary.points.push_back(gridToWorld(edgeStart, j));
                    boundary.points.push_back(gridToWorld(i - 1, j));
                }
                edgeStart = -1;
            }
        }
    }

    if (boundary.points.empty()) {
        for (int32_t j = 0; j < res; ++j) {
            for (int32_t i = 0; i < res; ++i) {
                if (isBorder(i, j)) {
                    boundary.points.push_back(gridToWorld(i, j));
                    break;
                }
            }
            if (!boundary.points.empty()) break;
        }
    }

    return boundary;
}

static std::vector<BooleanLoop> extractInnerLoops(const std::vector<uint8_t>& mask,
                                                    int32_t res,
                                                    const Aabb& bounds) {
    std::vector<BooleanLoop> innerLoops;

    const size_t pixelCount = static_cast<size_t>(res * res);
    std::vector<uint8_t> visited(pixelCount, 0);

    const int32_t di[] = {1, -1, 0, 0};
    const int32_t dj[] = {0, 0, -1, 1};

    for (int32_t j = 0; j < res; ++j) {
        for (int32_t i = 0; i < res; ++i) {
            size_t idx = static_cast<size_t>(j * res + i);
            if (mask[idx] != 0 || visited[idx]) continue;

            std::vector<size_t> component;
            std::vector<size_t> stack;
            stack.push_back(idx);
            visited[idx] = 1;
            bool touchesBorder = false;

            while (!stack.empty()) {
                size_t cur = stack.back();
                stack.pop_back();
                component.push_back(cur);

                int32_t ci = static_cast<int32_t>(cur % static_cast<size_t>(res));
                int32_t cj = static_cast<int32_t>(cur / static_cast<size_t>(res));

                if (ci <= 0 || ci >= res - 1 || cj <= 0 || cj >= res - 1) {
                    touchesBorder = true;
                }

                for (int d = 0; d < 4; ++d) {
                    int32_t ni = ci + di[d];
                    int32_t nj = cj + dj[d];
                    if (ni < 0 || ni >= res || nj < 0 || nj >= res) continue;
                    size_t nidx = static_cast<size_t>(nj * res + ni);
                    if (mask[nidx] != 0 || visited[nidx]) continue;
                    visited[nidx] = 1;
                    stack.push_back(nidx);
                }
            }

            if (touchesBorder) continue;

            std::vector<uint8_t> holeMask(pixelCount, 0);
            for (size_t k : component) {
                holeMask[k] = 1;
            }

            BooleanLoop holeBoundary = extractBoundary(holeMask, res, bounds);
            if (!holeBoundary.empty()) {
                std::reverse(holeBoundary.points.begin(), holeBoundary.points.end());
                innerLoops.push_back(std::move(holeBoundary));
            }
        }
    }

    return innerLoops;
}

BooleanRegion TrimBoolean::compute(
    const BooleanRegion& a,
    const BooleanRegion& b,
    BooleanOp op,
    const TrimBooleanOptions& opts) {

    BooleanRegion result;
    if (a.empty() && b.empty()) return result;

    Aabb boundsA = regionBounds(a);
    Aabb boundsB = regionBounds(b);

    Aabb combined;
    combined.min.x = std::min(boundsA.min.x, boundsB.min.x);
    combined.min.y = std::min(boundsA.min.y, boundsB.min.y);
    combined.max.x = std::max(boundsA.max.x, boundsB.max.x);
    combined.max.y = std::max(boundsA.max.y, boundsB.max.y);

    if (combined.min.x >= combined.max.x || combined.min.y >= combined.max.y) return result;

    float padX = (combined.max.x - combined.min.x) * 0.05f;
    float padY = (combined.max.y - combined.min.y) * 0.05f;
    combined.min.x -= padX;
    combined.max.x += padX;
    combined.min.y -= padY;
    combined.max.y += padY;

    int32_t res = std::max(opts.gridRes, 4);

    std::vector<uint8_t> maskA, maskB;
    rasterizeToGrid(a, combined, res, maskA);
    rasterizeToGrid(b, combined, res, maskB);

    std::vector<uint8_t> resultMask(static_cast<size_t>(res * res), 0);
    for (size_t k = 0; k < static_cast<size_t>(res * res); ++k) {
        bool inA = maskA[k] != 0;
        bool inB = maskB[k] != 0;
        bool inResult = false;
        switch (op) {
            case BooleanOp::Union:        inResult = inA || inB; break;
            case BooleanOp::Intersection: inResult = inA && inB; break;
            case BooleanOp::Difference:   inResult = inA && !inB; break;
        }
        resultMask[k] = inResult ? 1 : 0;
    }

    BooleanLoop outer = extractBoundary(resultMask, res, combined);
    if (!outer.empty()) {
        result.outerLoops.push_back(std::move(outer));
    }

    result.innerLoops = extractInnerLoops(resultMask, res, combined);

    return result;
}

} // namespace nexus::geometry
