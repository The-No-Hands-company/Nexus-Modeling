#pragma once

#include <nexus/render/Camera.h>

#include <algorithm>
#include <cmath>

namespace nexus::geometry::internal {

using Vec3 = nexus::render::Vec3;

inline double pointTriangleDistSq(const Vec3& p,
                                  const Vec3& a, const Vec3& b, const Vec3& c,
                                  double& outU, double& outV) noexcept;

inline double pointTriangleDistSq(const Vec3& p,
                                  const Vec3& a, const Vec3& b, const Vec3& c) noexcept {
    double u = 0.0;
    double v = 0.0;
    return pointTriangleDistSq(p, a, b, c, u, v);
}

inline double pointTriangleDistSq(const Vec3& p,
                                  const Vec3& a, const Vec3& b, const Vec3& c,
                                  double& outU, double& outV) noexcept {
    const auto e0x = static_cast<double>(b.x - a.x);
    const auto e0y = static_cast<double>(b.y - a.y);
    const auto e0z = static_cast<double>(b.z - a.z);

    const auto e1x = static_cast<double>(c.x - a.x);
    const auto e1y = static_cast<double>(c.y - a.y);
    const auto e1z = static_cast<double>(c.z - a.z);

    const auto vx = static_cast<double>(p.x - a.x);
    const auto vy = static_cast<double>(p.y - a.y);
    const auto vz = static_cast<double>(p.z - a.z);

    const double d00 = e0x * e0x + e0y * e0y + e0z * e0z;
    const double d01 = e0x * e1x + e0y * e1y + e0z * e1z;
    const double d11 = e1x * e1x + e1y * e1y + e1z * e1z;
    const double d20 = vx * e0x + vy * e0y + vz * e0z;
    const double d21 = vx * e1x + vy * e1y + vz * e1z;

    static constexpr double kEpsilon = 1e-30;

    const double denom = d00 * d11 - d01 * d01;

    double s = 0.0;
    double t = 0.0;

    if (std::abs(denom) > kEpsilon) {
        s = (d11 * d20 - d01 * d21) / denom;
        t = (d00 * d21 - d01 * d20) / denom;
    }

    if (s >= 0.0 && t >= 0.0 && s + t <= 1.0) {
        outU = s;
        outV = t;
        const double cx = vx - s * e0x - t * e1x;
        const double cy = vy - s * e0y - t * e1y;
        const double cz = vz - s * e0z - t * e1z;
        return cx * cx + cy * cy + cz * cz;
    }

    double bestDist = std::numeric_limits<double>::max();
    double bestS = 0.0;
    double bestT = 0.0;

    {
        double edgeT = std::clamp(d20 / (d00 + kEpsilon), 0.0, 1.0);
        double dx = vx - edgeT * e0x;
        double dy = vy - edgeT * e0y;
        double dz = vz - edgeT * e0z;
        double dSq = dx * dx + dy * dy + dz * dz;
        if (dSq < bestDist) {
            bestDist = dSq;
            bestS = edgeT;
            bestT = 0.0;
        }
    }

    {
        double edgeS = std::clamp(d21 / (d11 + kEpsilon), 0.0, 1.0);
        double dx = vx - edgeS * e1x;
        double dy = vy - edgeS * e1y;
        double dz = vz - edgeS * e1z;
        double dSq = dx * dx + dy * dy + dz * dz;
        if (dSq < bestDist) {
            bestDist = dSq;
            bestS = 0.0;
            bestT = edgeS;
        }
    }

    const double ebx = e0x - e1x;
    const double eby = e0y - e1y;
    const double ebz = e0z - e1z;
    const double d00b = ebx * ebx + eby * eby + ebz * ebz;
    const double d20b = (vx - e1x) * ebx + (vy - e1y) * eby + (vz - e1z) * ebz;

    {
        double edgeU = std::clamp(d20b / (d00b + kEpsilon), 0.0, 1.0);
        double dx = (vx - e1x) - edgeU * ebx;
        double dy = (vy - e1y) - edgeU * eby;
        double dz = (vz - e1z) - edgeU * ebz;
        double dSq = dx * dx + dy * dy + dz * dz;
        if (dSq < bestDist) {
            bestDist = dSq;
            bestS = 1.0 - edgeU;
            bestT = edgeU;
        }
    }

    {
        double dSq = vx * vx + vy * vy + vz * vz;
        if (dSq < bestDist) {
            bestDist = dSq;
            bestS = 0.0;
            bestT = 0.0;
        }
    }

    {
        double ex = vx - e0x;
        double ey = vy - e0y;
        double ez = vz - e0z;
        double dSq = ex * ex + ey * ey + ez * ez;
        if (dSq < bestDist) {
            bestDist = dSq;
            bestS = 1.0;
            bestT = 0.0;
        }
    }

    {
        double ex = vx - e1x;
        double ey = vy - e1y;
        double ez = vz - e1z;
        double dSq = ex * ex + ey * ey + ez * ez;
        if (dSq < bestDist) {
            bestDist = dSq;
            bestS = 0.0;
            bestT = 1.0;
        }
    }

    outU = bestS;
    outV = bestT;
    return bestDist;
}

} // namespace nexus::geometry::internal
