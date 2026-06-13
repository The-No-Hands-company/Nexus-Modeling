#include <nexus/geometry/SurfaceSurfaceIntersect.h>
#include <nexus/geometry/NurbsSurfaceClosestPoint.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEps = 1e-10f;

struct SeedPoint {
    float ua, va, ub, vb;
};

std::vector<SeedPoint> findSeeds(const NurbsSurface& a,
                                  const NurbsSurface& b,
                                  const IntersectSeedGrid& seedGrid) {
    std::vector<SeedPoint> seeds;
    auto domAU = a.domainU();
    auto domAV = a.domainV();

    NurbsSurfaceClosestPointOptions projOpts;
    projOpts.maxIter = 15;
    projOpts.tolerance = 1e-6f;

    for (int32_t i = 0; i < seedGrid.resU; ++i) {
        for (int32_t j = 0; j < seedGrid.resV; ++j) {
            float ua = domAU.first + (domAU.second - domAU.first) * static_cast<float>(i) / static_cast<float>(std::max(1, seedGrid.resU - 1));
            float va = domAV.first + (domAV.second - domAV.first) * static_cast<float>(j) / static_cast<float>(std::max(1, seedGrid.resV - 1));

            Vec3 pt = a.evaluate(ua, va);
            auto proj = NurbsSurfaceClosestPoint::project(b, pt, projOpts);
            if (!proj.converged) continue;

            float d2 = (pt.x - proj.point.x) * (pt.x - proj.point.x) +
                       (pt.y - proj.point.y) * (pt.y - proj.point.y) +
                       (pt.z - proj.point.z) * (pt.z - proj.point.z);

            Vec3 nA = a.derivativeU(ua, va).cross(a.derivativeV(ua, va));
            Vec3 nB = b.derivativeU(proj.u, proj.v).cross(b.derivativeV(proj.u, proj.v));
            float nALen = nA.length();
            float nBLen = nB.length();
            float angleThreshold = (nALen > kEps && nBLen > kEps) ?
                std::abs(nA.dot(nB) / (nALen * nBLen)) : 1.f;

            float seedThreshold = 0.01f * (1.f - angleThreshold) + 0.001f;
            if (d2 < seedThreshold) {
                SeedPoint s;
                s.ua = ua; s.va = va;
                s.ub = proj.u; s.vb = proj.v;

                bool duplicate = false;
                for (const auto& existing : seeds) {
                    float du = existing.ua - s.ua;
                    float dv = existing.va - s.va;
                    if (du*du + dv*dv < 1e-4f) { duplicate = true; break; }
                }
                if (!duplicate) seeds.push_back(s);
            }
        }
    }

    return seeds;
}

void newtonRefine(const NurbsSurface& a, const NurbsSurface& b,
                  float& ua, float& va, float& ub, float& vb,
                  int32_t maxIter = 20) {
    auto domAU = a.domainU();
    auto domAV = a.domainV();
    auto domBU = b.domainU();
    auto domBV = b.domainV();

    for (int32_t iter = 0; iter < maxIter; ++iter) {
        Vec3 pa = a.evaluate(ua, va);
        Vec3 pb = b.evaluate(ub, vb);
        Vec3 d = {pa.x - pb.x, pa.y - pb.y, pa.z - pb.z};

        Vec3 duA = a.derivativeU(ua, va);
        Vec3 dvA = a.derivativeV(ua, va);
        Vec3 duB = b.derivativeU(ub, vb);
        Vec3 dvB = b.derivativeV(ub, vb);

        float J00 = duA.dot(duA); float J01 = duA.dot(dvA);
        float J10 = dvA.dot(duA); float J11 = dvA.dot(dvA);
        float K00 = -duA.dot(duB); float K01 = -duA.dot(dvB);
        float K10 = -dvA.dot(duB); float K11 = -dvA.dot(dvB);

        float rhs0 = -d.dot(duA);
        float rhs1 = -d.dot(dvA);

        float delta_ua, delta_va, delta_ub, delta_vb;
        float det = J00 * J11 - J01 * J10;
        if (std::abs(det) > 1e-12f) {
            float s0 = (J11 * rhs0 - J01 * rhs1) / det;
            float s1 = (J00 * rhs1 - J10 * rhs0) / det;

            delta_ua = s0;
            delta_va = s1;
            delta_ub = (K00 * s0 + K10 * s1);
            delta_vb = (K01 * s0 + K11 * s1);
        } else {
            delta_ua = rhs0 * 0.5f;
            delta_va = rhs1 * 0.5f;
            delta_ub = -rhs0 * 0.5f;
            delta_vb = -rhs1 * 0.5f;
        }

        float maxDelta = std::max({std::abs(delta_ua), std::abs(delta_va),
                                    std::abs(delta_ub), std::abs(delta_vb)});
        if (maxDelta > 0.1f) {
            float scale = 0.1f / maxDelta;
            delta_ua *= scale; delta_va *= scale;
            delta_ub *= scale; delta_vb *= scale;
        }

        ua += delta_ua;
        va += delta_va;
        ub += delta_ub;
        vb += delta_vb;

        ua = std::clamp(ua, domAU.first, domAU.second);
        va = std::clamp(va, domAV.first, domAV.second);
        ub = std::clamp(ub, domBU.first, domBU.second);
        vb = std::clamp(vb, domBV.first, domBV.second);

        if (maxDelta < 1e-8f) break;
    }
}

bool marchStep(const NurbsSurface& a, const NurbsSurface& b,
               float& ua, float& va, float& ub, float& vb,
               float stepSize, bool& hitBoundary) {
    auto domAU = a.domainU();
    auto domAV = a.domainV();

    Vec3 duA = a.derivativeU(ua, va);
    Vec3 dvA = a.derivativeV(ua, va);
    Vec3 duB = b.derivativeU(ub, vb);
    Vec3 dvB = b.derivativeV(ub, vb);

    Vec3 nA = duA.cross(dvA);
    Vec3 nB = duB.cross(dvB);

    float nALen = nA.length();
    float nBLen = nB.length();
    if (nALen < kEps || nBLen < kEps) return false;

    nA = {nA.x / nALen, nA.y / nALen, nA.z / nALen};
    nB = {nB.x / nBLen, nB.y / nBLen, nB.z / nBLen};

    Vec3 dir = {
        nA.y * nB.z - nA.z * nB.y,
        nA.z * nB.x - nA.x * nB.z,
        nA.x * nB.y - nA.y * nB.x,
    };
    float dirLen = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (dirLen < kEps) return false;
    dir = {dir.x / dirLen, dir.y / dirLen, dir.z / dirLen};

    float duProj = dir.dot(duA);
    float dvProj = dir.dot(dvA);
    float duLenSq = duA.x*duA.x + duA.y*duA.y + duA.z*duA.z;
    float dvLenSq = dvA.x*dvA.x + dvA.y*dvA.y + dvA.z*dvA.z;

    float denom = duLenSq * dvLenSq;
    if (denom < kEps) return false;

    float deltaUa = stepSize * duProj / denom;
    float deltaVa = stepSize * dvProj / denom;

    float nu = ua + deltaUa;
    float nv = va + deltaVa;

    if (nu < domAU.first || nu > domAU.second || nv < domAV.first || nv > domAV.second) {
        hitBoundary = true;
        return false;
    }

    ua = nu;
    va = nv;

    auto proj = NurbsSurfaceClosestPoint::project(b, a.evaluate(ua, va));
    if (!proj.converged) return false;
    ub = proj.u;
    vb = proj.v;

    return true;
}

std::vector<Vec3> traceCurve(const NurbsSurface& a, const NurbsSurface& b,
                              float ua0, float va0, float ub0, float vb0,
                              int32_t maxSteps, float stepSize) {
    std::vector<Vec3> curve;
    curve.reserve(static_cast<size_t>(maxSteps + 1));

    float ua = ua0, va = va0, ub = ub0, vb = vb0;
    newtonRefine(a, b, ua, va, ub, vb);
    curve.push_back(a.evaluate(ua, va));

    for (int32_t step = 0; step < maxSteps; ++step) {
        bool hitBoundary = false;
        if (!marchStep(a, b, ua, va, ub, vb, stepSize, hitBoundary)) break;

        newtonRefine(a, b, ua, va, ub, vb, 10);
        curve.push_back(a.evaluate(ua, va));

        if (hitBoundary) break;
    }

    return curve;
}

} // namespace

std::vector<std::vector<Vec3>> SurfaceSurfaceIntersect::intersect(
    const NurbsSurface& a,
    const NurbsSurface& b,
    const IntersectSeedGrid& seedGrid,
    int32_t marchSteps,
    float stepSize) {

    std::vector<std::vector<Vec3>> curves;

    if (!a.isValid() || !b.isValid()) return curves;

    auto seeds = findSeeds(a, b, seedGrid);

    for (const auto& seed : seeds) {
        auto curve = traceCurve(a, b, seed.ua, seed.va, seed.ub, seed.vb,
                                 marchSteps, stepSize);
        if (curve.size() >= 3) {
            curves.push_back(std::move(curve));
        }
    }

    return curves;
}

} // namespace nexus::geometry
