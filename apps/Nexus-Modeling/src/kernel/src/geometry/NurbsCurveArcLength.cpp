#include <nexus/geometry/NurbsCurveArcLength.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
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

float integrateSpeedOnSpan(const NurbsCurve& curve,
                           float uA, float uB, int32_t qp) {
    float half = 0.5f * (uB - uA);
    float mid  = 0.5f * (uA + uB);
    float sum  = 0.f;
    for (int32_t i = 0; i < qp; ++i) {
        float u  = mid + half * kGL5Abscissae[i];
        Vec3  d  = curve.derivative(u, 1);
        float sp = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        sum += half * kGL5Weights[i] * sp;
    }
    return sum;
}

} // namespace

NurbsCurveArcLengthResult NurbsCurveArcLength::compute(
    const NurbsCurve& curve, const NurbsCurveArcLengthOptions& qp) {
    NurbsCurveArcLengthResult result;
    if (!curve.isValid()) return result;

    auto [uMin, uMax] = curve.domain();
    const auto& knots = curve.knots();
    int32_t p = curve.degree();
    int32_t n = curve.controlPointCount();

    float total = 0.f;
    result.cumulativeLengths.push_back(0.f);
    result.knotValues.push_back(uMin);

    for (int32_t i = p; i < n; ++i) {
        float a = knots[static_cast<size_t>(i)];
        float b = knots[static_cast<size_t>(i + 1)];
        if (b <= a) continue;
        float len = integrateSpeedOnSpan(curve, a, b, qp.quadraturePoints);
        total += len;
        result.cumulativeLengths.push_back(total);
        result.knotValues.push_back(b);
    }

    result.totalLength = total;
    return result;
}

float NurbsCurveArcLength::parameterAtLength(
    const NurbsCurve& curve, float targetLength,
    const NurbsCurveArcLengthResult& cache,
    const NurbsCurveArcLengthOptions& qp) {
    if (!curve.isValid()) return 0.f;

    auto [uMin, uMax] = curve.domain();
    if (targetLength <= 0.f) return uMin;

    NurbsCurveArcLengthResult localCache;
    const NurbsCurveArcLengthResult& cl = cache.cumulativeLengths.empty() ? localCache : cache;
    if (cache.cumulativeLengths.empty())
        localCache = compute(curve, qp);

    if (targetLength >= cl.totalLength) return uMax;

    // Binary search on cumLengths
    const auto& clens = cl.cumulativeLengths;
    const auto& kvals = cl.knotValues;
    int32_t lo = 0, hi = static_cast<int32_t>(clens.size()) - 1;
    while (lo < hi - 1) {
        int32_t mid = (lo + hi) / 2;
        if (clens[mid] < targetLength) lo = mid;
        else hi = mid;
    }

    float segStart = clens[lo];
    float segEnd   = clens[hi];
    float segLen   = segEnd - segStart;
    float t = (segLen > 1e-10f) ? (targetLength - segStart) / segLen : 0.f;
    return kvals[lo] + t * (kvals[hi] - kvals[lo]);
}

} // namespace nexus::geometry
