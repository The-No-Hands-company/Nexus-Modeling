#include <nexus/geometry/NurbsCurveOffset.h>
#include <nexus/geometry/NurbsCurveFit.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline Vec3 normalize(const Vec3& v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-8f) return {0.f, 0.f, 1.f};
    return {v.x / len, v.y / len, v.z / len};
}

inline Vec3 scale(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

float magnitudeSq(const Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float magnitude(const Vec3& v) {
    return std::sqrt(magnitudeSq(v));
}

} // namespace

std::vector<Vec3> NurbsCurveOffset::offset(const NurbsCurve& curve,
                                            const NurbsCurveOffsetOptions& opts) {
    if (!curve.isValid() || opts.samples < 2) return {};

    auto [uMin, uMax] = curve.domain();
    int32_t n = opts.samples;
    float du = (uMax - uMin) / static_cast<float>(n - 1);

    Vec3 pNormal = normalize(opts.planeNormal);
    std::vector<Vec3> result;
    result.reserve(static_cast<size_t>(n));

    for (int32_t i = 0; i < n; ++i) {
        float u = uMin + static_cast<float>(i) * du;
        Vec3 C  = curve.evaluate(u);
        Vec3 Cp = curve.derivative(u, 1);

        float cpLen2 = magnitudeSq(Cp);
        if (cpLen2 < 1e-12f) {
            result.push_back(C);
            continue;
        }
        float cpLen = std::sqrt(cpLen2);
        Vec3 tangent = {Cp.x / cpLen, Cp.y / cpLen, Cp.z / cpLen};

        Vec3 inPlaneNormal = normalize(cross(pNormal, tangent));

        Vec3 offsetPt{
            C.x + opts.distance * inPlaneNormal.x,
            C.y + opts.distance * inPlaneNormal.y,
            C.z + opts.distance * inPlaneNormal.z,
        };
        result.push_back(offsetPt);
    }
    return result;
}

NurbsCurve NurbsCurveOffset::fitToNurbs(const NurbsCurve& curve,
                                         const NurbsCurveOffsetOptions& opts,
                                         NurbsCurveOffsetReport* report) {
    NurbsCurveOffsetReport localReport;
    localReport.code  = NurbsCurveOffsetDiagnostic::Success;
    localReport.valid = false;

    if (!curve.isValid()) {
        localReport.addMessage("input curve is invalid");
        if (report) *report = localReport;
        return {};
    }
    if (opts.samples < 2) {
        localReport.addMessage("need at least 2 samples");
        if (report) *report = localReport;
        return {};
    }

    auto [uMin, uMax] = curve.domain();
    int32_t n = opts.samples;
    float du = (uMax - uMin) / static_cast<float>(n - 1);

    Vec3 pNormal = normalize(opts.planeNormal);
    std::vector<Vec3> offsetPts;
    offsetPts.reserve(static_cast<size_t>(n));

    float minRadiusOfCurvature = std::numeric_limits<float>::max();

    for (int32_t i = 0; i < n; ++i) {
        float u = uMin + static_cast<float>(i) * du;
        Vec3 C   = curve.evaluate(u);
        Vec3 Cp  = curve.derivative(u, 1);
        Vec3 Cpp = curve.derivative(u, 2);

        float cpLen2 = magnitudeSq(Cp);
        if (cpLen2 < 1e-12f) {
            offsetPts.push_back(C);
            continue;
        }
        float cpLen = std::sqrt(cpLen2);
        Vec3 tangent = scale(Cp, 1.f / cpLen);

        Vec3 cpCrossCpp = cross(Cp, Cpp);
        float num = cpLen2 * cpLen; // |Cp|^3
        float den = magnitude(cpCrossCpp);
        if (den > 1e-10f) {
            float r = num / den;
            if (r < minRadiusOfCurvature)
                minRadiusOfCurvature = r;
        }

        Vec3 inPlaneNormal = normalize(cross(pNormal, tangent));

        Vec3 offsetPt{
            C.x + opts.distance * inPlaneNormal.x,
            C.y + opts.distance * inPlaneNormal.y,
            C.z + opts.distance * inPlaneNormal.z,
        };
        offsetPts.push_back(offsetPt);
    }

    float absDist = std::abs(opts.distance);
    if (absDist > 0.f && minRadiusOfCurvature < std::numeric_limits<float>::max()
        && absDist > minRadiusOfCurvature) {
        localReport.code = static_cast<NurbsCurveOffsetDiagnostic>(
            static_cast<uint32_t>(localReport.code) |
            static_cast<uint32_t>(NurbsCurveOffsetDiagnostic::SelfIntersectWarning));
        std::ostringstream oss;
        oss << "offset distance (" << absDist
            << ") exceeds minimum radius of curvature ("
            << minRadiusOfCurvature
            << ") — offset curve may self-intersect";
        localReport.addMessage(oss.str());
    }

    NurbsCurveFitOptions fitOpts;
    fitOpts.degree    = opts.fitDegree;
    fitOpts.tolerance = opts.fitTolerance;
    fitOpts.controlPoints = std::min(n, opts.fitDegree + 5);

    auto fitResult = NurbsCurveFit::fit(offsetPts, fitOpts);

    if (!fitResult.curve.isValid()) {
        localReport.code = static_cast<NurbsCurveOffsetDiagnostic>(
            static_cast<uint32_t>(localReport.code) |
            static_cast<uint32_t>(NurbsCurveOffsetDiagnostic::FitFailed));
        localReport.addMessage("NURBS curve fit failed to produce a valid curve");
        if (report) *report = localReport;
        return {};
    }

    localReport.valid = true;
    if (report) *report = localReport;
    return fitResult.curve;
}

} // namespace nexus::geometry
