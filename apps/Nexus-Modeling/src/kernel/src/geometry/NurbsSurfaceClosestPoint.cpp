#include <nexus/geometry/NurbsSurfaceClosestPoint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEpsilon = 1e-12f;

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float lengthSq(const Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline float length(const Vec3& v) {
    return std::sqrt(lengthSq(v));
}

inline Vec3 sub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline float clamp(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

} // namespace

NurbsSurfaceClosestPointResult NurbsSurfaceClosestPoint::project(
    const NurbsSurface& surface, const Vec3& query,
    const NurbsSurfaceClosestPointOptions& opts) {

    NurbsSurfaceClosestPointResult result;
    if (!surface.isValid()) return result;

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();

    float bestU = uMin, bestV = vMin;
    float bestDist = std::numeric_limits<float>::max();

    float du = (uMax - uMin) / static_cast<float>(std::max(1, opts.gridSizeU - 1));
    float dv = (vMax - vMin) / static_cast<float>(std::max(1, opts.gridSizeV - 1));

    for (int32_t i = 0; i < opts.gridSizeU; ++i) {
        float u = uMin + static_cast<float>(i) * du;
        for (int32_t j = 0; j < opts.gridSizeV; ++j) {
            float v = vMin + static_cast<float>(j) * dv;
            Vec3 s = surface.evaluate(u, v);
            float d2 = lengthSq(sub(s, query));
            if (d2 < bestDist) {
                bestDist = d2;
                bestU = u;
                bestV = v;
            }
        }
    }

    float currU = bestU, currV = bestV;
    float stepLen = std::min(du, dv) * opts.stepDamping;

    for (int32_t iter = 0; iter < opts.maxIter; ++iter) {
        Vec3  C   = surface.evaluate(currU, currV);
        Vec3  Cu  = surface.derivativeU(currU, currV);
        Vec3  Cv  = surface.derivativeV(currU, currV);
        Vec3  diff = sub(C, query);

        float f0 = dot(diff, diff);
        float fU = 2.f * dot(diff, Cu);
        float fV = 2.f * dot(diff, Cv);

        float gradNorm = std::sqrt(fU * fU + fV * fV);
        if (gradNorm < opts.tolerance) {
            result.converged = true;
            break;
        }

        // Line search with damping
        float alpha = stepLen;
        float bestObj = f0;
        float bestStepU = 0.f, bestStepV = 0.f;
        for (int32_t ls = 0; ls < 8; ++ls) {
            float stepU = -alpha * fU / gradNorm;
            float stepV = -alpha * fV / gradNorm;
            float nextU = clamp(currU + stepU, uMin, uMax);
            float nextV = clamp(currV + stepV, vMin, vMax);
            Vec3  nextC = surface.evaluate(nextU, nextV);
            float nextF = lengthSq(sub(nextC, query));
            if (nextF < bestObj) {
                bestObj = nextF;
                bestStepU = stepU;
                bestStepV = stepV;
            }
            alpha *= 0.5f;
        }

        float nextU = clamp(currU + bestStepU, uMin, uMax);
        float nextV = clamp(currV + bestStepV, vMin, vMax);

        float deltaParam = std::abs(nextU - currU) + std::abs(nextV - currV);
        currU = nextU;
        currV = nextV;

        if (deltaParam < opts.minStep) {
            result.converged = true;
            break;
        }

        result.iterations = iter + 1;
    }

    result.u        = currU;
    result.v        = currV;
    result.point    = surface.evaluate(currU, currV);
    result.distance = length(sub(result.point, query));
    return result;
}

} // namespace nexus::geometry
