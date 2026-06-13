#include <nexus/geometry/NurbsIntersection.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

std::vector<IntersectionPoint> NurbsIntersection::intersectSurfaceCurve(
    const NurbsSurface& surface,
    const NurbsCurve& curve,
    float tolerance) {

    std::vector<IntersectionPoint> results;
    if (!surface.isValid() || !curve.isValid()) return results;

    auto [umin, umax] = surface.domainU();
    auto [vmin, vmax] = surface.domainV();
    auto [tmin, tmax] = curve.domain();

    const int coarseSamples = 32;
    std::vector<IntersectionPoint> candidates;

    for (int ti = 0; ti <= coarseSamples; ++ti) {
        float t = tmin + (tmax - tmin) * static_cast<float>(ti) / static_cast<float>(coarseSamples);
        Vec3 cp = curve.evaluate(t);

        float bestDist = std::numeric_limits<float>::max();
        float bestU = 0.5f, bestV = 0.5f;
        for (int ui = 0; ui <= coarseSamples; ++ui) {
            float u = umin + (umax - umin) * static_cast<float>(ui) / static_cast<float>(coarseSamples);
            for (int vi = 0; vi <= coarseSamples; ++vi) {
                float v = vmin + (vmax - vmin) * static_cast<float>(vi) / static_cast<float>(coarseSamples);
                Vec3 sp = surface.evaluate(u, v);
                float d = (sp - cp).lengthSq();
                if (d < bestDist) {
                    bestDist = d;
                    bestU = u;
                    bestV = v;
                }
            }
        }

        if (std::sqrt(bestDist) < 0.1f) {
            candidates.push_back({bestU, bestV, static_cast<double>(t), {}});
        }
    }

    for (auto& cand : candidates) {
        float u = cand.u, v = cand.v;
        double td = cand.t;

        for (int iter = 0; iter < 20; ++iter) {
            u = std::clamp(u, umin, umax);
            v = std::clamp(v, vmin, vmax);
            td = std::clamp(td, static_cast<double>(tmin) + 1e-9, static_cast<double>(tmax) - 1e-9);

            Vec3 Sp = surface.evaluate(u, v);
            Vec3 Su = surface.derivativeU(u, v);
            Vec3 Sv = surface.derivativeV(u, v);
            Vec3 Cp = curve.evaluate(static_cast<float>(td));
            Vec3 Cprime = curve.derivative(static_cast<float>(td));

            Vec3 F = Sp - Cp;

            float error = std::sqrt(F.lengthSq());
            if (error < tolerance) break;

            float a11 = Su.x, a12 = Sv.x, a13 = -Cprime.x;
            float a21 = Su.y, a22 = Sv.y, a23 = -Cprime.y;
            float a31 = Su.z, a32 = Sv.z, a33 = -Cprime.z;

            float damping = 0.5f;
            float rx = -F.x, ry = -F.y, rz = -F.z;

            float det = a11 * (a22*a33 - a23*a32)
                      - a12 * (a21*a33 - a23*a31)
                      + a13 * (a21*a32 - a22*a31);

            if (std::fabs(det) < 1e-12f) break;

            float invDet = 1.0f / det;
            float du = invDet * (rx * (a22*a33 - a23*a32) + a12 * (a23*rz - ry*a33) + a13 * (ry*a32 - a22*rz));
            float dv = invDet * (a11 * (ry*a33 - a23*rz) + rx * (a23*a31 - a21*a33) + a13 * (a21*rz - ry*a31));
            float dt = invDet * (a11 * (a22*rz - ry*a32) + a12 * (ry*a31 - a21*rz) + rx * (a21*a32 - a22*a31));

            u += du * damping;
            v += dv * damping;
            td += static_cast<double>(dt) * static_cast<double>(damping);

            if (std::fabs(du) < 1e-8f && std::fabs(dv) < 1e-8f && std::fabs(dt) < 1e-12)
                break;
        }

        u = std::clamp(u, umin, umax);
        v = std::clamp(v, vmin, vmax);
        td = std::clamp(td, static_cast<double>(tmin), static_cast<double>(tmax));

        cand.u = u;
        cand.v = v;
        cand.t = td;
        cand.point = surface.evaluate(u, v);
    }

    for (const auto& c : candidates) {
        bool dup = false;
        for (const auto& r : results) {
            float du = c.u - r.u, dv = c.v - r.v;
            float dt = static_cast<float>(c.t - r.t);
            if (du*du + dv*dv < 1e-4f && std::fabs(dt) < 1e-3f) {
                dup = true;
                break;
            }
        }
        if (!dup) results.push_back(c);
    }

    return results;
}

std::vector<NurbsCurveCurveIntersect> NurbsIntersection::intersectCurves(
    const NurbsCurve& a,
    const NurbsCurve& b,
    float tolerance) {

    std::vector<NurbsCurveCurveIntersect> results;
    if (!a.isValid() || !b.isValid()) return results;

    auto [taMin, taMax] = a.domain();
    auto [tbMin, tbMax] = b.domain();

    const int coarseSamples = 32;
    std::vector<NurbsCurveCurveIntersect> candidates;

    for (int ia = 0; ia <= coarseSamples; ++ia) {
        float ta = taMin + (taMax - taMin) * static_cast<float>(ia) / static_cast<float>(coarseSamples);
        Vec3 pa = a.evaluate(ta);

        float bestDist = std::numeric_limits<float>::max();
        float bestTb = 0.5f;
        for (int ib = 0; ib <= coarseSamples; ++ib) {
            float tb = tbMin + (tbMax - tbMin) * static_cast<float>(ib) / static_cast<float>(coarseSamples);
            Vec3 pb = b.evaluate(tb);
            float d = (pa - pb).lengthSq();
            if (d < bestDist) {
                bestDist = d;
                bestTb = tb;
            }
        }

        if (std::sqrt(bestDist) < 0.1f) {
            candidates.push_back({static_cast<double>(ta), static_cast<double>(bestTb), {}});
        }
    }

    for (int ib = 0; ib <= coarseSamples; ++ib) {
        float tb = tbMin + (tbMax - tbMin) * static_cast<float>(ib) / static_cast<float>(coarseSamples);
        Vec3 pb = b.evaluate(tb);

        float bestDist = std::numeric_limits<float>::max();
        float bestTa = 0.5f;
        for (int ia = 0; ia <= coarseSamples; ++ia) {
            float ta = taMin + (taMax - taMin) * static_cast<float>(ia) / static_cast<float>(coarseSamples);
            Vec3 pa = a.evaluate(ta);
            float d = (pa - pb).lengthSq();
            if (d < bestDist) {
                bestDist = d;
                bestTa = ta;
            }
        }

        if (std::sqrt(bestDist) < 0.1f) {
            candidates.push_back({static_cast<double>(bestTa), static_cast<double>(tb), {}});
        }
    }

    for (auto& cand : candidates) {
        double ta = cand.ta;
        double tb = cand.tb;

        for (int iter = 0; iter < 20; ++iter) {
            ta = std::clamp(ta, static_cast<double>(taMin) + 1e-9, static_cast<double>(taMax) - 1e-9);
            tb = std::clamp(tb, static_cast<double>(tbMin) + 1e-9, static_cast<double>(tbMax) - 1e-9);

            Vec3 Ca = a.evaluate(static_cast<float>(ta));
            Vec3 Cda = a.derivative(static_cast<float>(ta));
            Vec3 Cb = b.evaluate(static_cast<float>(tb));
            Vec3 Cdb = b.derivative(static_cast<float>(tb));

            Vec3 F = Ca - Cb;
            float error = std::sqrt(F.lengthSq());
            if (error < tolerance) break;

            float a11 = Cda.dot(Cda);
            float a12 = -Cda.dot(Cdb);
            float a22 = Cdb.dot(Cdb);

            Vec3 diff = Cb - Ca;
            float b1 = Cda.dot(diff);
            float b2 = -Cdb.dot(diff);

            float det = a11 * a22 - a12 * a12;
            if (std::fabs(det) < 1e-12f) break;

            float invDet = 1.0f / det;
            float dt = invDet * (b1 * a22 - a12 * b2);
            float ds = invDet * (a11 * b2 - a12 * b1);

            float maxStep = 0.1f;
            float stepLen = std::sqrt(dt * dt + ds * ds);
            float damping = 1.0f;
            if (stepLen > maxStep) {
                damping = maxStep / stepLen;
            }

            ta += static_cast<double>(dt) * static_cast<double>(damping);
            tb += static_cast<double>(ds) * static_cast<double>(damping);

            if (std::fabs(dt) < 1e-8f && std::fabs(ds) < 1e-8f)
                break;
        }

        ta = std::clamp(ta, static_cast<double>(taMin), static_cast<double>(taMax));
        tb = std::clamp(tb, static_cast<double>(tbMin), static_cast<double>(tbMax));

        cand.ta = ta;
        cand.tb = tb;
        cand.point = a.evaluate(static_cast<float>(ta));
    }

    for (const auto& c : candidates) {
        bool dup = false;
        for (const auto& r : results) {
            float dta = static_cast<float>(c.ta - r.ta);
            float dtb = static_cast<float>(c.tb - r.tb);
            if (dta * dta < 1e-4f && dtb * dtb < 1e-4f) {
                dup = true;
                break;
            }
        }
        if (!dup) results.push_back(c);
    }

    return results;
}

} // namespace nexus::geometry
