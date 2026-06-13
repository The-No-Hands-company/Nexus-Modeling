#include <nexus/geometry/NurbsCurveClosestPoint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

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

NurbsCurveClosestPointResult NurbsCurveClosestPoint::project(
    const NurbsCurve& curve, const Vec3& query,
    const NurbsCurveClosestPointOptions& opts) {

    NurbsCurveClosestPointResult result;
    if (!curve.isValid()) return result;

    auto [uMin, uMax] = curve.domain();
    float bestU = uMin;
    float bestDist = std::numeric_limits<float>::max();

    float du = (uMax - uMin) / static_cast<float>(std::max(1, opts.samples - 1));
    for (int32_t i = 0; i < opts.samples; ++i) {
        float u = uMin + static_cast<float>(i) * du;
        Vec3 p = curve.evaluate(u);
        float d2 = lengthSq(sub(p, query));
        if (d2 < bestDist) {
            bestDist = d2;
            bestU = u;
        }
    }

    float currU = bestU;
    float stepSize = du * opts.stepDamping;

    for (int32_t iter = 0; iter < opts.maxIter; ++iter) {
        Vec3 C    = curve.evaluate(currU);
        Vec3 Cp   = curve.derivative(currU, 1);
        Vec3 Cpp  = curve.derivative(currU, 2);
        Vec3 diff = sub(C, query);

        float f_prime  = dot(Cp, diff);
        float f_double = dot(Cp, Cp) + dot(Cpp, diff);

        if (std::abs(f_prime) < opts.tolerance) {
            result.converged = true;
            break;
        }

        float step = 0.f;
        if (std::abs(f_double) > 1e-10f) {
            step = -f_prime / f_double;
        } else {
            step = -f_prime * stepSize / std::sqrt(f_prime * f_prime + 1e-10f);
        }

        // Clip step
        float maxStep = stepSize;
        step = clamp(step, -maxStep, maxStep);

        float nextU = clamp(currU + step, uMin, uMax);
        float delta = std::abs(nextU - currU);
        currU = nextU;

        if (delta < opts.minStep) {
            result.converged = true;
            break;
        }
        result.iterations = iter + 1;
    }

    result.param    = currU;
    result.point    = curve.evaluate(currU);
    result.distance = length(sub(result.point, query));
    return result;
}

} // namespace nexus::geometry
