#include <nexus/geometry/SurfacePrimitives.h>
#include <nexus/geometry/BezierCurve.h>
#include <nexus/geometry/ProfileRevolve.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// ── Ruled Surface ──────────────────────────────────────────────────────────

NurbsSurface makeRuledSurface(const NurbsCurve& c0, const NurbsCurve& c1)
{
    if (!c0.isValid() || !c1.isValid()) return {};

    // Use c0's degree and aspect ratio; promote c1 to match.
    int32_t degU = c0.degree();
    int32_t degV = 1; // linear in V
    int32_t nU = c0.controlPointCount();
    int32_t nV = 2;   // two rows

    std::vector<float> knotU = c0.knots();
    std::vector<float> knotV = {0.f, 0.f, 1.f, 1.f};

    // Resample c1 at c0's parameter values to match CP count.
    auto c0Domain = c0.domain();
    std::vector<Vec3> pts1;
    pts1.reserve(nU);
    for (int32_t i = 0; i < nU; ++i) {
        float t = c0Domain.first +
            (c0Domain.second - c0Domain.first) * static_cast<float>(i) / static_cast<float>(nU - 1);
        pts1.push_back(c1.evaluate(t));
    }

    // Build CP grid: row 0 = c0 CPs, row 1 = sampled c1 points.
    std::vector<Vec3> cps;
    cps.reserve(static_cast<size_t>(nU) * static_cast<size_t>(nV));

    const auto& c0CPs = c0.controlPoints();
    for (int32_t i = 0; i < nU; ++i) {
        cps.push_back(c0CPs[static_cast<size_t>(i)]);
    }
    for (int32_t i = 0; i < nU; ++i) {
        cps.push_back(pts1[static_cast<size_t>(i)]);
    }

    return NurbsSurface(degU, degV, knotU, knotV, cps, nU, nV);
}

// ── Pipe/Tube Surface ──────────────────────────────────────────────────────

NurbsSurface makeTubeSurface(const NurbsCurve& centerline, float radius,
                              uint32_t samples)
{
    if (!centerline.isValid() || radius <= 0.f || samples < 3) return {};

    // Sample the centerline.
    int32_t pathCPs = centerline.controlPointCount();
    int32_t nPath = std::max(pathCPs, 4);

    auto dom = centerline.domain();
    std::vector<Vec3> pathPts(static_cast<size_t>(nPath));
    std::vector<Vec3> pathTans(static_cast<size_t>(nPath));
    for (int32_t i = 0; i < nPath; ++i) {
        float t = dom.first +
            (dom.second - dom.first) * static_cast<float>(i) / static_cast<float>(nPath - 1);
        pathPts[static_cast<size_t>(i)] = centerline.evaluate(t);
        pathTans[static_cast<size_t>(i)] = centerline.derivative(t);
    }

    int32_t nU = static_cast<int32_t>(samples);
    int32_t nV = nPath;
    int32_t degU = std::min(3, nU - 1);
    int32_t degV = std::min(3, nV - 1);
    degU = std::max(degU, 1);
    degV = std::max(degV, 1);

    std::vector<Vec3> cps(static_cast<size_t>(nU) * static_cast<size_t>(nV));

    for (int32_t j = 0; j < nV; ++j) {
        Vec3 tan = pathTans[static_cast<size_t>(j)].normalize();
        Vec3 u, v;
        if (std::abs(tan.x) < 0.9f)
            u = Vec3{1, 0, 0}.cross(tan).normalize();
        else
            u = Vec3{0, 1, 0}.cross(tan).normalize();
        v = tan.cross(u).normalize();

        Vec3 center = pathPts[static_cast<size_t>(j)];
        for (int32_t i = 0; i < nU; ++i) {
            float angle = 2.f * static_cast<float>(std::numbers::pi) *
                static_cast<float>(i) / static_cast<float>(nU);
            Vec3 pt = center + (u * std::cos(angle) + v * std::sin(angle)) * radius;
            cps[static_cast<size_t>(i * nV + j)] = pt;
        }
    }

    std::vector<float> knotU(static_cast<size_t>(nU + degU + 1));
    for (int32_t j = 0; j <= degU; ++j) knotU[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nU - degU; ++j)
        knotU[static_cast<size_t>(degU + j)] = static_cast<float>(j) / static_cast<float>(nU - degU);
    for (int32_t j = 0; j <= degU; ++j) knotU[knotU.size()-1-static_cast<size_t>(j)] = 1.f;

    std::vector<float> knotV(static_cast<size_t>(nV + degV + 1));
    for (int32_t j = 0; j <= degV; ++j) knotV[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nV - degV; ++j)
        knotV[static_cast<size_t>(degV + j)] = static_cast<float>(j) / static_cast<float>(nV - degV);
    for (int32_t j = 0; j <= degV; ++j) knotV[knotV.size()-1-static_cast<size_t>(j)] = 1.f;

    return NurbsSurface(degU, degV, std::move(knotU), std::move(knotV),
                        std::move(cps), nU, nV);
}

// ── Isoparametric Curve Extraction ─────────────────────────────────────────

NurbsCurve isoCurveU(const NurbsSurface& surf, float u)
{
    if (!surf.isValid()) return {};
    int32_t nV = surf.controlPointCountV();
    if (nV < 2) return {};

    int32_t span = surf.findSpanU(u);
    if (span < 0) return {};

    // Extract C(v) = S(u, v) by evaluating at constant u.
    std::vector<Vec3> cps;
    cps.reserve(nV);
    for (int32_t j = 0; j < nV; ++j)
        cps.push_back(surf.evaluate(u,
            surf.domainV().first + (surf.domainV().second - surf.domainV().first) *
            static_cast<float>(j) / static_cast<float>(nV - 1)));

    int32_t deg = surf.degreeV();
    std::vector<float> knots = surf.knotV();
    return NurbsCurve(deg, std::move(knots), std::move(cps));
}

NurbsCurve isoCurveV(const NurbsSurface& surf, float v)
{
    if (!surf.isValid()) return {};
    int32_t nU = surf.controlPointCountU();
    if (nU < 2) return {};

    int32_t span = surf.findSpanV(v);
    if (span < 0) return {};

    std::vector<Vec3> cps;
    cps.reserve(nU);
    for (int32_t i = 0; i < nU; ++i)
        cps.push_back(surf.evaluate(
            surf.domainU().first + (surf.domainU().second - surf.domainU().first) *
            static_cast<float>(i) / static_cast<float>(nU - 1),
            v));

    int32_t deg = surf.degreeU();
    std::vector<float> knots = surf.knotU();
    return NurbsCurve(deg, std::move(knots), std::move(cps));
}

// ── Primitive Surfaces ───────────────────────────────────────────────────

NurbsSurface makePlaneSurface(const Vec3& point, const Vec3& normal, float extent)
{
    Vec3 n = normal.normalize();
    Vec3 u, v;
    if (std::abs(n.x) < 0.9f) {
        u = Vec3{1, 0, 0}.cross(n).normalize();
    } else {
        u = Vec3{0, 1, 0}.cross(n).normalize();
    }
    v = n.cross(u).normalize();

    float e = extent * 0.5f;
    return NurbsSurface(1, 1,
        {0.f, 0.f, 1.f, 1.f}, {0.f, 0.f, 1.f, 1.f},
        {point - u*e - v*e, point + u*e - v*e,
         point - u*e + v*e, point + u*e + v*e}, 2, 2);
}

NurbsSurface makeSphereSurface(const Vec3& center, float radius)
{
    if (radius <= 0.f) return {};
    // Approximate sphere as a revolved half-circle.
    NurbsCurve profile = ConicCurve::arc(center, {1,0,0}, {0,0,1}, radius, 180.f);
    if (!profile.isValid()) return {};

    RevolveDesc desc;
    desc.axisOrigin = center;
    desc.axisDirection = {0, 0, 1};
    desc.angularSamples = 16;
    desc.outputAsNurbsSurface = true;
    NurbsSurface result;
    (void)RevolveOperation::revolve(profile, desc, result, nullptr);
    return result;
}

NurbsSurface makeTorusSurface(const Vec3& center, const Vec3& axis,
                                float majorRadius, float minorRadius)
{
    if (majorRadius <= 0.f || minorRadius <= 0.f) return {};
    // Torus = circle revolved around axis.
    Vec3 n = axis.normalize();
    Vec3 u, v;
    if (std::abs(n.x) < 0.9f) {
        u = Vec3{1, 0, 0}.cross(n).normalize();
    } else {
        u = Vec3{0, 1, 0}.cross(n).normalize();
    }
    v = n.cross(u).normalize();

    NurbsCurve circle = ConicCurve::circle(center + u * majorRadius, n, minorRadius);
    if (!circle.isValid()) return {};

    RevolveDesc desc;
    desc.axisOrigin = center;
    desc.axisDirection = axis;
    desc.angularSamples = 24;
    desc.outputAsNurbsSurface = true;
    NurbsSurface result;
    (void)RevolveOperation::revolve(circle, desc, result, nullptr);
    return result;
}

NurbsSurface makeOffsetSurface(const NurbsSurface& surf, float distance)
{
    // Sample the surface, offset points along normal, re-fit.
    if (!surf.isValid()) return {};
    int32_t nU = std::max(surf.controlPointCountU(), 8);
    int32_t nV = std::max(surf.controlPointCountV(), 8);
    auto domU = surf.domainU(), domV = surf.domainV();

    std::vector<Vec3> cps(static_cast<size_t>(nU) * static_cast<size_t>(nV));
    for (int32_t i = 0; i < nU; ++i) {
        float u = domU.first + (domU.second - domU.first) * static_cast<float>(i) / static_cast<float>(nU - 1);
        for (int32_t j = 0; j < nV; ++j) {
            float v = domV.first + (domV.second - domV.first) * static_cast<float>(j) / static_cast<float>(nV - 1);
            Vec3 pt = surf.evaluate(u, v);
            Vec3 du = surf.derivativeU(u, v);
            Vec3 dv = surf.derivativeV(u, v);
            Vec3 normal = du.cross(dv).normalize();
            cps[static_cast<size_t>(i * nV + j)] = pt + normal * distance;
        }
    }

    int32_t degU = std::min(3, nU - 1), degV = std::min(3, nV - 1);
    degU = std::max(degU, 1); degV = std::max(degV, 1);
    std::vector<float> kU(static_cast<size_t>(nU + degU + 1));
    std::vector<float> kV(static_cast<size_t>(nV + degV + 1));
    for (int32_t j = 0; j <= degU; ++j) kU[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nU - degU; ++j)
        kU[static_cast<size_t>(degU + j)] = static_cast<float>(j) / static_cast<float>(nU - degU);
    for (int32_t j = 0; j <= degU; ++j) kU[kU.size()-1-static_cast<size_t>(j)] = 1.f;
    for (int32_t j = 0; j <= degV; ++j) kV[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nV - degV; ++j)
        kV[static_cast<size_t>(degV + j)] = static_cast<float>(j) / static_cast<float>(nV - degV);
    for (int32_t j = 0; j <= degV; ++j) kV[kV.size()-1-static_cast<size_t>(j)] = 1.f;

    return NurbsSurface(degU, degV, std::move(kU), std::move(kV), std::move(cps), nU, nV);
}

NurbsSurface makeCoonsPatch(const NurbsCurve& c0, const NurbsCurve& c1,
                              const NurbsCurve& d0, const NurbsCurve& d1)
{
    // Coons patch: S(u,v) = (1-v)*C0(u) + v*C1(u) + (1-u)*D0(v) + u*D1(v)
    //                       - [(1-u)(1-v)*P00 + u(1-v)*P10 + (1-u)v*P01 + uv*P11]
    if (!c0.isValid() || !c1.isValid() || !d0.isValid() || !d1.isValid()) return {};

    int32_t nU = c0.controlPointCount();
    int32_t nV = d0.controlPointCount();
    auto domU = c0.domain(), domV = d0.domain();

    std::vector<Vec3> cps(static_cast<size_t>(nU) * static_cast<size_t>(nV));
    for (int32_t i = 0; i < nU; ++i) {
        float u = domU.first + (domU.second - domU.first) * static_cast<float>(i) / static_cast<float>(nU - 1);
        Vec3 c0u = c0.evaluate(u), c1u = c1.evaluate(u);
        for (int32_t j = 0; j < nV; ++j) {
            float v = domV.first + (domV.second - domV.first) * static_cast<float>(j) / static_cast<float>(nV - 1);
            Vec3 d0v = d0.evaluate(v), d1v = d1.evaluate(v);
            Vec3 pt = (c0u * (1.f - v) + c1u * v) + (d0v * (1.f - u) + d1v * u);
            Vec3 corner = c0.evaluate(domU.first) * (1.f-u)*(1.f-v)
                       + c1.evaluate(domU.first) * u*(1.f-v)
                       + d0.evaluate(domV.second) * (1.f-u)*v
                       + d1.evaluate(domV.second) * u*v;
            cps[static_cast<size_t>(i * nV + j)] = pt - corner;
        }
    }

    int32_t degU = std::min(3, nU-1), degV = std::min(3, nV-1);
    degU = std::max(degU, 1); degV = std::max(degV, 1);
    std::vector<float> kU(static_cast<size_t>(nU+degU+1)), kV(static_cast<size_t>(nV+degV+1));
    for (int32_t j=0; j<=degU; ++j) kU[static_cast<size_t>(j)]=0.f;
    for (int32_t j=1; j<nU-degU; ++j) kU[static_cast<size_t>(degU+j)]=static_cast<float>(j)/static_cast<float>(nU-degU);
    for (int32_t j=0; j<=degU; ++j) kU[kU.size()-1-static_cast<size_t>(j)]=1.f;
    for (int32_t j=0; j<=degV; ++j) kV[static_cast<size_t>(j)]=0.f;
    for (int32_t j=1; j<nV-degV; ++j) kV[static_cast<size_t>(degV+j)]=static_cast<float>(j)/static_cast<float>(nV-degV);
    for (int32_t j=0; j<=degV; ++j) kV[kV.size()-1-static_cast<size_t>(j)]=1.f;
    return NurbsSurface(degU, degV, std::move(kU), std::move(kV), std::move(cps), nU, nV);
}

} // namespace nexus::geometry
