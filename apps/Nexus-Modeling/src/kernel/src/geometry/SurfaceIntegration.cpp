#include <nexus/geometry/SurfaceIntegration.h>

#include <cmath>
#include <cstdint>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline float norm(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

} // namespace

SurfaceIntegrationResult SurfaceIntegration::compute(
    const NurbsSurface& surface, const SurfaceIntegrationOptions& opts) {
    SurfaceIntegrationResult result;
    if (!surface.isValid()) return result;

    int32_t su = opts.samplesU;
    int32_t sv = opts.samplesV;
    if (su < 2 || sv < 2) return result;

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();

    float du = (uMax - uMin) / static_cast<float>(su);
    float dv = (vMax - vMin) / static_cast<float>(sv);

    float area = 0.f;
    float volume = 0.f;

    for (int32_t i = 0; i < su; ++i) {
        float u = uMin + (static_cast<float>(i) + 0.5f) * du;
        for (int32_t j = 0; j < sv; ++j) {
            float v = vMin + (static_cast<float>(j) + 0.5f) * dv;

            Vec3 S = surface.evaluate(u, v);
            Vec3 Su = surface.derivativeU(u, v);
            Vec3 Sv = surface.derivativeV(u, v);
            Vec3 n  = cross(Su, Sv);
            float nLen = norm(n);

            // Area = ∫∫|S_u × S_v| du dv
            area += nLen * du * dv;

            // Signed volume via divergence theorem:
            // ∭ div(F) dV = ∫∫ F·n dA with F(x,y,z) = (x, y, z)/3
            // div(F) = 1, so V = 1/3 ∫∫ S·n dA
            volume += dot(S, n) * du * dv;
        }
    }

    result.area   = area;
    result.signedVolume = volume / 3.f;
    return result;
}

} // namespace nexus::geometry
