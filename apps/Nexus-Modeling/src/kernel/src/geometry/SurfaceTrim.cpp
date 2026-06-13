#include <nexus/geometry/SurfaceTrim.h>

#include <algorithm>
#include <cstdint>

namespace nexus::geometry {

bool SurfaceTrim::isEmpty() const {
    return loops.empty();
}

size_t SurfaceTrim::curveCount() const {
    size_t total = 0;
    for (const auto& loop : loops) {
        total += loop.curves.size();
    }
    return total;
}

void SurfaceTrim::addLoop(const TrimLoop& loop) {
    loops.push_back(loop);
}

bool SurfaceTrim::segmentRayIntersect(const Vec2& a, const Vec2& b,
                                       float pu, float pv) noexcept {
    if (!((a.v <= pv && b.v > pv) || (b.v <= pv && a.v > pv)))
        return false;
    if (a.u < pu && b.u < pu)
        return false;
    float t = (pv - a.v) / (b.v - a.v);
    float intersectU = a.u + t * (b.u - a.u);
    return intersectU >= pu;
}

static std::vector<Vec2> sampleNurbsForTrim(const NurbsCurve& curve) {
    auto [uMin, uMax] = curve.domain();
    constexpr int kSamples = 64;
    std::vector<Vec2> pts;
    pts.reserve(kSamples);
    for (int i = 0; i < kSamples; ++i) {
        float t = uMin + (uMax - uMin) * static_cast<float>(i) / static_cast<float>(kSamples - 1);
        auto pt = curve.evaluate(t);
        pts.push_back({pt.x, pt.y});
    }
    return pts;
}

bool SurfaceTrim::containsPoint(float u, float v) const {
    if (loops.empty()) return true;

    bool insideAnyOuter = false;
    bool insideAnyHole = false;
    for (const auto& loop : loops) {
        int intersections = 0;
        for (const auto& curve : loop.curves) {
            std::vector<Vec2> sampled;
            const std::vector<Vec2>* pts = &curve.points;
            if (curve.nurbsCurve.has_value()) {
                sampled = sampleNurbsForTrim(*curve.nurbsCurve);
                pts = &sampled;
            }
            if (pts->size() < 2) continue;
            for (size_t i = 0; i < pts->size() - 1; ++i) {
                if (segmentRayIntersect((*pts)[i], (*pts)[i + 1], u, v))
                    ++intersections;
            }
            if (curve.closed && pts->size() >= 2) {
                if (segmentRayIntersect(pts->back(), pts->front(), u, v))
                    ++intersections;
            }
        }
        bool loopInside = (intersections % 2) == 1;
        if (loop.isOuterLoop) {
            if (loopInside) insideAnyOuter = true;
        } else {
            if (loopInside) insideAnyHole = true;
        }
    }
    return insideAnyOuter && !insideAnyHole;
}

} // namespace nexus::geometry
