#include <nexus/geometry/SurfaceCurvature.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEpsilon = 1e-10f;
constexpr float kH       = 1e-4f;

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline Vec3 normalize(const Vec3& v) {
    float len = std::sqrt(dot(v, v));
    if (len < kEpsilon) return {0.f, 0.f, 1.f};
    return {v.x / len, v.y / len, v.z / len};
}

} // namespace

void SurfaceCurvature::computeAt(const NurbsSurface& surface,
                                  float u, float v, CurvatureSample& out) {
    out.u = u;
    out.v = v;
    out.point = surface.evaluate(u, v);

    // Finite-difference second derivatives
    float h = kH;

    Vec3 Su = surface.derivativeU(u, v);
    Vec3 Sv = surface.derivativeV(u, v);

    // Second derivatives via central differences on derivatives
    Vec3 Suu;
    {
        Vec3 d1 = surface.derivativeU(u + h, v);
        Vec3 d0 = surface.derivativeU(u - h, v);
        Suu = {(d1.x - d0.x) / (2.f * h),
               (d1.y - d0.y) / (2.f * h),
               (d1.z - d0.z) / (2.f * h)};
    }

    Vec3 Svv;
    {
        Vec3 d1 = surface.derivativeV(u, v + h);
        Vec3 d0 = surface.derivativeV(u, v - h);
        Svv = {(d1.x - d0.x) / (2.f * h),
               (d1.y - d0.y) / (2.f * h),
               (d1.z - d0.z) / (2.f * h)};
    }

    Vec3 Suv;
    {
        Vec3 d1 = surface.derivativeU(u, v + h);
        Vec3 d0 = surface.derivativeU(u, v - h);
        Suv = {(d1.x - d0.x) / (2.f * h),
               (d1.y - d0.y) / (2.f * h),
               (d1.z - d0.z) / (2.f * h)};
    }

    Vec3 N = normalize(cross(Su, Sv));
    out.normal = N;

    // First fundamental form coefficients
    float E = dot(Su, Su);
    float F = dot(Su, Sv);
    float G = dot(Sv, Sv);

    // Second fundamental form coefficients
    float L = dot(Suu, N);
    float M = dot(Suv, N);
    float Nc = dot(Svv, N);

    float denomFG = E * G - F * F;
    if (std::abs(denomFG) < kEpsilon) {
        out.gaussianCurvature = 0.f;
        out.meanCurvature     = 0.f;
        out.principalK1       = 0.f;
        out.principalK2       = 0.f;
        return;
    }

    // Gaussian curvature K = (L*Nc - M^2) / (E*G - F^2)
    out.gaussianCurvature = (L * Nc - M * M) / denomFG;

    // Mean curvature H = (E*Nc - 2*F*M + G*L) / (2*(E*G - F^2))
    out.meanCurvature = (E * Nc - 2.f * F * M + G * L) / (2.f * denomFG);

    // Principal curvatures: k1,k2 = H ± sqrt(H² - K)
    float disc = out.meanCurvature * out.meanCurvature - out.gaussianCurvature;
    if (disc < 0.f) disc = 0.f;
    float sqrtD = std::sqrt(disc);
    out.principalK1 = out.meanCurvature + sqrtD;
    out.principalK2 = out.meanCurvature - sqrtD;
}

std::vector<CurvatureSample> SurfaceCurvature::sample(
    const NurbsSurface& surface, int32_t samplesU, int32_t samplesV) {
    std::vector<CurvatureSample> result;
    if (!surface.isValid() || samplesU < 2 || samplesV < 2) return result;

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();

    float du = (uMax - uMin) / static_cast<float>(samplesU - 1);
    float dv = (vMax - vMin) / static_cast<float>(samplesV - 1);

    result.reserve(static_cast<size_t>(samplesU * samplesV));
    for (int32_t i = 0; i < samplesU; ++i) {
        float u = uMin + static_cast<float>(i) * du;
        for (int32_t j = 0; j < samplesV; ++j) {
            float v = vMin + static_cast<float>(j) * dv;
            CurvatureSample samp;
            computeAt(surface, u, v, samp);
            result.push_back(samp);
        }
    }
    return result;
}

} // namespace nexus::geometry
