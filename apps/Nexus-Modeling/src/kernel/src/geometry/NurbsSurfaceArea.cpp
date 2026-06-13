#include <nexus/geometry/NurbsSurfaceArea.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kGL5Abscissae[5] = {
    -0.906179845938664f, -0.538469310105683f, 0.f,
     0.538469310105683f,  0.906179845938664f
};
constexpr float kGL5Weights[5] = {
    0.236926885056189f, 0.478628670499366f, 0.568888888888889f,
    0.478628670499366f, 0.236926885056189f
};

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline float norm(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float integrateOnCell(const NurbsSurface& surface,
                      float uA, float uB, float vA, float vB,
                      int32_t qp) {
    float halfDU = 0.5f * (uB - uA);
    float halfDV = 0.5f * (vB - vA);
    float uMid = 0.5f * (uA + uB);
    float vMid = 0.5f * (vA + vB);

    float sum = 0.f;
    for (int32_t i = 0; i < qp; ++i) {
        float u = uMid + halfDU * kGL5Abscissae[i];
        float wi = kGL5Weights[i] * halfDU;
        for (int32_t j = 0; j < qp; ++j) {
            float v = vMid + halfDV * kGL5Abscissae[j];
            float wj = kGL5Weights[j] * halfDV;

            Vec3 Su = surface.derivativeU(u, v);
            Vec3 Sv = surface.derivativeV(u, v);
            Vec3 n  = cross(Su, Sv);
            sum += wi * wj * norm(n);
        }
    }
    return sum;
}

} // namespace

NurbsSurfaceAreaResult NurbsSurfaceArea::compute(
    const NurbsSurface& surface, const NurbsSurfaceAreaOptions& opts) {
    NurbsSurfaceAreaResult result;
    if (!surface.isValid()) return result;

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();
    return subPatch(surface, uMin, uMax, vMin, vMax, opts);
}

NurbsSurfaceAreaResult NurbsSurfaceArea::subPatch(
    const NurbsSurface& surface,
    float u0, float u1, float v0, float v1,
    const NurbsSurfaceAreaOptions& opts) {
    NurbsSurfaceAreaResult result;
    if (!surface.isValid()) return result;

    const auto& ku = surface.knotU();
    const auto& kv = surface.knotV();
    int32_t nU = surface.controlPointCountU();
    int32_t nV = surface.controlPointCountV();
    int32_t pu = surface.degreeU();
    int32_t pv = surface.degreeV();
    int32_t qp = opts.quadraturePoints;

    // Per knot-span integration
    for (int32_t i = pu; i < nU; ++i) {
        float cellU0 = std::max(u0, ku[i]);
        float cellU1 = std::min(u1, ku[i + 1]);
        if (cellU1 <= cellU0) continue;

        for (int32_t j = pv; j < nV; ++j) {
            float cellV0 = std::max(v0, kv[j]);
            float cellV1 = std::min(v1, kv[j + 1]);
            if (cellV1 <= cellV0) continue;

            float area = integrateOnCell(surface, cellU0, cellU1,
                                         cellV0, cellV1, qp);
            result.totalArea += area;
            result.subPatchAreas.push_back(area);
        }
    }
    return result;
}

} // namespace nexus::geometry
