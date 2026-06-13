#include <nexus/geometry/SurfaceContinuity.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEpsilon = 1e-10f;

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline Vec3 sub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline float norm(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

inline Vec3 normalize(const Vec3& v) {
    float l = norm(v);
    if (l < kEpsilon) return {0.f, 0.f, 1.f};
    return {v.x / l, v.y / l, v.z / l};
}

float radToDeg(float rad) { return rad * 57.29577951308232f; }

void getEdgeUV(const NurbsSurface& surface, SurfaceEdge edge,
               float t, float& outU, float& outV) {
    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();
    switch (edge) {
    case SurfaceEdge::UMin:
        outU = uMin; outV = vMin + t * (vMax - vMin); break;
    case SurfaceEdge::UMax:
        outU = uMax; outV = vMin + t * (vMax - vMin); break;
    case SurfaceEdge::VMin:
        outU = uMin + t * (uMax - uMin); outV = vMin; break;
    case SurfaceEdge::VMax:
        outU = uMin + t * (uMax - uMin); outV = vMax; break;
    }
}

Vec3 edgeNormal(const NurbsSurface& surface, SurfaceEdge edge, float u, float v) {
    Vec3 Su = surface.derivativeU(u, v);
    Vec3 Sv = surface.derivativeV(u, v);
    Vec3 N  = normalize(cross(Su, Sv));
    if (edge == SurfaceEdge::UMin || edge == SurfaceEdge::UMax) {
        // Cross-boundary normal: should align
    }
    return N;
}

bool edgesMatch(SurfaceEdge a, SurfaceEdge b) {
    return (a == b) ||
           (a == SurfaceEdge::UMin && b == SurfaceEdge::UMax) ||
           (a == SurfaceEdge::UMax && b == SurfaceEdge::UMin) ||
           (a == SurfaceEdge::VMin && b == SurfaceEdge::VMax) ||
           (a == SurfaceEdge::VMax && b == SurfaceEdge::VMin);
}

} // namespace

SurfaceContinuityResult SurfaceContinuity::analyze(
    const NurbsSurface& surfaceA, SurfaceEdge edgeA,
    const NurbsSurface& surfaceB, SurfaceEdge edgeB,
    const SurfaceContinuityOptions& opts) {
    SurfaceContinuityResult result;
    if (!surfaceA.isValid() || !surfaceB.isValid()) return result;
    if (!edgesMatch(edgeA, edgeB)) return result;

    int32_t nSamples = std::max(2, opts.samples);
    result.sampleParams.reserve(static_cast<size_t>(nSamples));
    result.positionalGaps.reserve(static_cast<size_t>(nSamples));
    result.angularDevs.reserve(static_cast<size_t>(nSamples));

    float maxPosGap = 0.f;
    float maxAngDev = 0.f;
    float sumPosGap = 0.f;
    float sumAngDev = 0.f;

    for (int32_t i = 0; i < nSamples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(nSamples - 1);
        float u1, v1, u2, v2;
        getEdgeUV(surfaceA, edgeA, t, u1, v1);
        getEdgeUV(surfaceB, edgeB, t, u2, v2);

        Vec3 pa = surfaceA.evaluate(u1, v1);
        Vec3 pb = surfaceB.evaluate(u2, v2);

        float gap = norm(sub(pa, pb));
        result.positionalGaps.push_back(gap);
        result.sampleParams.push_back(t);
        maxPosGap = std::max(maxPosGap, gap);
        sumPosGap += gap;

        Vec3 na = edgeNormal(surfaceA, edgeA, u1, v1);
        Vec3 nb = edgeNormal(surfaceB, edgeB, u2, v2);

        float cosAng = dot(na, nb);
        cosAng = std::max(-1.f, std::min(1.f, cosAng));
        float angDev = std::acos(cosAng);
        float angDeg = radToDeg(angDev);
        result.angularDevs.push_back(angDeg);
        maxAngDev = std::max(maxAngDev, angDeg);
        sumAngDev += angDeg;
    }

    result.maxPositionalGap = maxPosGap;
    result.maxAngularDevDeg = maxAngDev;
    result.avgPositionalGap = sumPosGap / static_cast<float>(nSamples);
    result.avgAngularDevDeg = sumAngDev / static_cast<float>(nSamples);

    float g0Tol = 1e-4f;
    float g1Tol = 1.f; // 1 degree
    result.g0Continuous = maxPosGap < g0Tol;
    result.g1Continuous = maxPosGap < g0Tol && maxAngDev < g1Tol;

    return result;
}

} // namespace nexus::geometry
