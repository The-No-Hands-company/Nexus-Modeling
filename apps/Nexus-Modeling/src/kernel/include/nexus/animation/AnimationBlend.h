#pragma once
// ── Nexus Animation — AnimationBlend

#include <nexus/animation/AnimationCore.h>

#include <cstdint>

namespace nexus::animation {

class AnimationBlend {
public:
    [[nodiscard]] static Pose blend(const Pose& poseA,
                                     const Pose& poseB,
                                     float factor);
};

} // namespace nexus::animation
