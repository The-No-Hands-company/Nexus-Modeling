#include <nexus/geometry/SurfaceCurveProjection.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

SurfaceCurveProjection::Result SurfaceCurveProjection::projectCurve(
    const NurbsSurface& surface,
    const NurbsCurve& curve,
    int32_t samples) {

    Result result;
    if (!surface.isValid() || !curve.isValid() || samples < 2) return result;

    auto cDom = curve.domain();
    auto domU = surface.domainU();
    auto domV = surface.domainV();

    result.params.resize(static_cast<size_t>(samples));
    result.projectedPoints.resize(static_cast<size_t>(samples));
    result.maxError = 0.f;

    int32_t searchRes = 32;

    for (int32_t s = 0; s < samples; ++s) {
        float t = cDom.first + (cDom.second - cDom.first) * static_cast<float>(s) / static_cast<float>(samples - 1);
        Vec3 curvePt = curve.evaluate(t);

        float bestDist = std::numeric_limits<float>::max();
        float bestU = (domU.first + domU.second) * 0.5f;
        float bestV = (domV.first + domV.second) * 0.5f;

        for (int32_t i = 0; i < searchRes; ++i) {
            for (int32_t j = 0; j < searchRes; ++j) {
                float u = domU.first + (domU.second - domU.first) * static_cast<float>(i) / static_cast<float>(searchRes - 1);
                float v = domV.first + (domV.second - domV.first) * static_cast<float>(j) / static_cast<float>(searchRes - 1);
                Vec3 sp = surface.evaluate(u, v);
                float d = (sp - curvePt).lengthSq();
                if (d < bestDist) {
                    bestDist = d;
                    bestU = u;
                    bestV = v;
                }
            }
        }

        float u = bestU;
        float v = bestV;

        for (int iter = 0; iter < 16; ++iter) {
            Vec3 sp = surface.evaluate(u, v);
            Vec3 du = surface.derivativeU(u, v);
            Vec3 dv = surface.derivativeV(u, v);

            Vec3 diff = curvePt - sp;

            float E = du.dot(du);
            float F = du.dot(dv);
            float G = dv.dot(dv);

            float det = E * G - F * F;
            if (std::abs(det) < 1e-10f) break;

            float duDot = diff.dot(du);
            float dvDot = diff.dot(dv);

            float deltaU = (G * duDot - F * dvDot) / det;
            float deltaV = (E * dvDot - F * duDot) / det;

            float step = 0.5f;
            u += deltaU * step;
            v += deltaV * step;

            u = std::clamp(u, domU.first, domU.second);
            v = std::clamp(v, domV.first, domV.second);

            if (std::abs(deltaU) < 1e-6f && std::abs(deltaV) < 1e-6f) break;
        }

        Vec3 projected = surface.evaluate(u, v);
        float error = (projected - curvePt).length();
        if (error > result.maxError) result.maxError = error;

        result.params[static_cast<size_t>(s)] = {u, v};
        result.projectedPoints[static_cast<size_t>(s)] = projected;
    }

    return result;
}

} // namespace nexus::geometry
