#pragma once
// ─── Nexus Geometry ── NurbsCurveArcLength ──────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct NurbsCurveArcLengthOptions {
    int32_t quadraturePoints = 5;
};

struct NurbsCurveArcLengthResult {
    float totalLength = 0.f;
    std::vector<float> cumulativeLengths;
    std::vector<float> knotValues;
};

class NurbsCurveArcLength {
public:
    static NurbsCurveArcLengthResult compute(
        const NurbsCurve& curve,
        const NurbsCurveArcLengthOptions& qp = {});

    static float parameterAtLength(
        const NurbsCurve& curve,
        float targetLength,
        const NurbsCurveArcLengthResult& cache = {},
        const NurbsCurveArcLengthOptions& qp = {});
};

} // namespace nexus::geometry
