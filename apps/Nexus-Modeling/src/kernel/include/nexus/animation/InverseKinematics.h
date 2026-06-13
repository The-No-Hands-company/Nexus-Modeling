#pragma once

#include <nexus/animation/AnimationCore.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <string>

namespace nexus::animation {

struct IKSettings {
    uint32_t maxIterations = 10;
    float    tolerance     = 1e-4f;
    float    damping       = 0.5f;

    /// Per-bone joint angle limit in radians. Empty = unlimited.
    /// Each entry is {boneIndex, maxAngleRadians}. Angle is clamped per CCD
    /// iteration using the axis-angle rotation magnitude.
    std::vector<std::pair<uint32_t, float>> jointLimits;
};

struct IKResult {
    bool ok = false;
    std::string error;
    uint32_t iterations = 0;
    float residual = 0.0f;
};

class InverseKinematics {
public:
    [[nodiscard]] static IKResult solveCCD(
        Skeleton& skeleton,
        Pose& pose,
        uint32_t endEffectorBone,
        const nexus::render::Vec3& target,
        const IKSettings& settings = {});
};

} // namespace nexus::animation
