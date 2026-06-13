#pragma once

#include <nexus/render/Camera.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nexus::geometry::internal {

using Vec3 = nexus::render::Vec3;

struct Eigen3x3 {
    float val[3] = {0.f, 0.f, 0.f};
    Vec3  vec[3] = {};
};

inline Eigen3x3 solveEigenSymmetric3x3(const float m[6]) noexcept {
    static constexpr float kTwoPiOver3 = 2.0943951023931953f;
    static constexpr float kEpsilon    = 1e-12f;

    const float a00 = m[0];
    const float a11 = m[1];
    const float a22 = m[2];
    const float a01 = m[3];
    const float a02 = m[4];
    const float a12 = m[5];

    const float tr = a00 + a11 + a22;
    const float mean = tr / 3.0f;

    const float b00 = a00 - mean;
    const float b11 = a11 - mean;
    const float b22 = a22 - mean;

    const float p = (b00 * b00 + b11 * b11 + b22 * b22 +
                     2.0f * (a01 * a01 + a02 * a02 + a12 * a12)) / 6.0f;

    const float q = 0.5f * (
        b00 * (b11 * b22 - a12 * a12) -
        a01 * (a01 * b22 - a12 * a02) +
        a02 * (a01 * a12 - b11 * a02)
    );

    Eigen3x3 result;

    if (p < kEpsilon) {
        result.val[0] = a00;
        result.val[1] = a11;
        result.val[2] = a22;
        result.vec[0] = Vec3{1.f, 0.f, 0.f};
        result.vec[1] = Vec3{0.f, 1.f, 0.f};
        result.vec[2] = Vec3{0.f, 0.f, 1.f};
        return result;
    }

    const float sqrtP = std::sqrt(p);
    const float phiArg = q / (p * sqrtP);
    const float phi = std::acos(std::clamp(phiArg, -1.0f, 1.0f)) / 3.0f;

    const float twoSqrtP = 2.0f * sqrtP;
    result.val[0] = mean + twoSqrtP * std::cos(phi);
    result.val[1] = mean + twoSqrtP * std::cos(phi + kTwoPiOver3);
    result.val[2] = mean + twoSqrtP * std::cos(phi - kTwoPiOver3);

    for (int k = 0; k < 3; ++k) {
        const float lambda = result.val[k];

        const float r0x = a00 - lambda;
        const float r0y = a01;
        const float r0z = a02;

        const float r1x = a01;
        const float r1y = a11 - lambda;
        const float r1z = a12;

        const float r2x = a02;
        const float r2y = a12;
        const float r2z = a22 - lambda;

        Vec3 cr01 = Vec3{r0y * r1z - r0z * r1y,
                         r0z * r1x - r0x * r1z,
                         r0x * r1y - r0y * r1x};
        float n01 = cr01.lengthSq();

        Vec3 cr02 = Vec3{r0y * r2z - r0z * r2y,
                         r0z * r2x - r0x * r2z,
                         r0x * r2y - r0y * r2x};
        float n02 = cr02.lengthSq();

        Vec3 cr12 = Vec3{r1y * r2z - r1z * r2y,
                         r1z * r2x - r1x * r2z,
                         r1x * r2y - r1y * r2x};
        float n12 = cr12.lengthSq();

        Vec3 ev;
        if (n01 >= n02 && n01 >= n12) {
            ev = cr01;
        } else if (n02 >= n12) {
            ev = cr02;
        } else {
            ev = cr12;
        }

        float lenSq = ev.lengthSq();
        if (lenSq > kEpsilon) {
            result.vec[k] = ev * (1.0f / std::sqrt(lenSq));
        } else {
            result.vec[k] = Vec3{1.f, 0.f, 0.f};
        }
    }

    return result;
}

} // namespace nexus::geometry::internal
