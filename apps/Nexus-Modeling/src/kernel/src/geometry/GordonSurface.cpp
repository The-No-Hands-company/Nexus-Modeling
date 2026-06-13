#include <nexus/geometry/GordonSurface.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

static float lagrangeL(int32_t i, const std::vector<float>& xs, float x, int32_t n) {
    float result = 1.f;
    for (int32_t j = 0; j < n; ++j) {
        if (j == i) continue;
        if (std::abs(xs[static_cast<size_t>(i)] - xs[static_cast<size_t>(j)]) < 1e-10f) return 0.f;
        result *= (x - xs[static_cast<size_t>(j)]) /
                  (xs[static_cast<size_t>(i)] - xs[static_cast<size_t>(j)]);
    }
    return result;
}

static Vec3 interpolateAlong(int32_t nCurves, const std::vector<float>& vParams,
                             float v, const std::vector<NurbsCurve>& curves,
                             float u) {
    Vec3 result{0.f, 0.f, 0.f};
    for (int32_t i = 0; i < nCurves; ++i) {
        Vec3 pt = curves[static_cast<size_t>(i)].evaluate(u);
        float l = lagrangeL(i, vParams, v, nCurves);
        result.x += pt.x * l;
        result.y += pt.y * l;
        result.z += pt.z * l;
    }
    return result;
}

static Vec3 tensorProductCorrection(const std::vector<NurbsCurve>& uCurves,
                                     const std::vector<NurbsCurve>&,
                                     const std::vector<float>& uParams,
                                     const std::vector<float>& vParams,
                                     int32_t nU, int32_t nV,
                                     float u, float v) {
    Vec3 correction{0.f, 0.f, 0.f};
    for (int32_t i = 0; i < nU; ++i) {
        for (int32_t j = 0; j < nV; ++j) {
            Vec3 uiCurvePt = uCurves[static_cast<size_t>(i)].evaluate(uParams[static_cast<size_t>(j)]);
            float li = lagrangeL(i, uParams, u, nU);
            float lj = lagrangeL(j, vParams, v, nV);
            correction.x -= uiCurvePt.x * li * lj;
            correction.y -= uiCurvePt.y * li * lj;
            correction.z -= uiCurvePt.z * li * lj;
        }
    }
    return correction;
}

Mesh GordonSurface::build(const CurveFamily& uCurves,
                           const CurveFamily& vCurves,
                           const GordonSurfaceOptions& opts) {
    Mesh result;
    auto& attrs = result.attributes();
    auto& topo  = result.topology();

    int32_t nU = static_cast<int32_t>(uCurves.curves.size());
    int32_t nV = static_cast<int32_t>(vCurves.curves.size());

    if (nU < 2 || nV < 2 || opts.samplesU < 2 || opts.samplesV < 2) return result;

    std::vector<float> uParams(nU);
    std::vector<float> vParams(nV);
    for (int32_t i = 0; i < nU; ++i) uParams[static_cast<size_t>(i)] = static_cast<float>(i) / static_cast<float>(nU - 1);
    for (int32_t j = 0; j < nV; ++j) vParams[static_cast<size_t>(j)] = static_cast<float>(j) / static_cast<float>(nV - 1);

    auto uDom = uCurves.curves[0].domain();
    auto vDom = vCurves.curves[0].domain();
    float du = (uDom.second - uDom.first) / static_cast<float>(opts.samplesU - 1);
    float dv = (vDom.second - vDom.first) / static_cast<float>(opts.samplesV - 1);

    std::vector<Vec3> positions;
    positions.reserve(static_cast<size_t>(opts.samplesU) * static_cast<size_t>(opts.samplesV));

    for (int32_t i = 0; i < opts.samplesU; ++i) {
        float u = uDom.first + du * static_cast<float>(i);
        for (int32_t j = 0; j < opts.samplesV; ++j) {
            float v = vDom.first + dv * static_cast<float>(j);

            Vec3 uvSurface = interpolateAlong(nV, vParams, v, vCurves.curves, u);
            Vec3 vuSurface = interpolateAlong(nU, uParams, u, uCurves.curves, v);
            Vec3 correction = tensorProductCorrection(uCurves.curves, vCurves.curves,
                                                       uParams, vParams, nU, nV, u, v);

            Vec3 pt;
            pt.x = uvSurface.x + vuSurface.x + correction.x;
            pt.y = uvSurface.y + vuSurface.y + correction.y;
            pt.z = uvSurface.z + vuSurface.z + correction.z;

            positions.push_back(pt);
        }
    }

    for (int32_t i = 0; i < opts.samplesU - 1; ++i) {
        for (int32_t j = 0; j < opts.samplesV - 1; ++j) {
            uint32_t a = static_cast<uint32_t>(i * opts.samplesV + j);
            uint32_t b = static_cast<uint32_t>(i * opts.samplesV + j + 1);
            uint32_t c = static_cast<uint32_t>((i + 1) * opts.samplesV + j + 1);
            uint32_t d = static_cast<uint32_t>((i + 1) * opts.samplesV + j);
            Face f1; f1.indices = {a, b, c}; topo.addFace(f1);
            Face f2; f2.indices = {a, c, d}; topo.addFace(f2);
        }
    }

    attrs.setPositions(positions);
    if (!result.computeVertexNormals()) return result;
    return result;
}

} // namespace nexus::geometry
