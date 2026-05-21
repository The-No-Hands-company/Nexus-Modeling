#pragma once

#include <nexus/eval/EvalGraph.h>
#include <nexus/animation/AnimationCore.h>
#include <nexus/animation/SkeletonRetargeter.h>

namespace nexus::eval {

enum class AnimationNodeKind : uint32_t {
    Blend = 0x00020000u,
    Sample = 0x00020001u,
    Retarget = 0x00020002u,
};

class AnimationBlendNode {
public:
    [[nodiscard]] static constexpr AnimationNodeKind kind() noexcept {
        return AnimationNodeKind::Blend;
    }

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "AnimationBlend";
    }

    // Blends two animation clips with deterministic linear interpolation.
    // weight ∈ [0, 1]: 0 = full clipA, 1 = full clipB.
    // Returns true on success; false if clips have mismatched skeleton requirements.
    [[nodiscard]] static bool compute(
        const nexus::animation::AnimationClip& clipA,
        const nexus::animation::AnimationClip& clipB,
        float weight,
        nexus::animation::AnimationClip& outClip);
};

class AnimationSampleNode {
public:
    [[nodiscard]] static constexpr AnimationNodeKind kind() noexcept {
        return AnimationNodeKind::Sample;
    }

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "AnimationSample";
    }

    // Samples an animation clip at a specific time with deterministic quantization.
    // timeSeconds must be normalized to clip duration; wrap/clamp is handled by caller.
    // Returns a Pose that can flow into skinning or scene application.
    [[nodiscard]] static bool compute(
        const nexus::animation::AnimationClip& clip,
        const nexus::animation::Skeleton& skeleton,
        float timeSeconds,
        nexus::animation::Pose& outPose);
};

class AnimationRetargetNode {
public:
    [[nodiscard]] static constexpr AnimationNodeKind kind() noexcept {
        return AnimationNodeKind::Retarget;
    }

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "AnimationRetarget";
    }

    // Retargets an animation pose from source skeleton to target skeleton deterministically.
    // Uses bone name matching via SkeletonRetargeter; unmapped bones remain at identity.
    // Returns true on success; false if retargeter cannot be built from skeleton mismatch.
    [[nodiscard]] static bool compute(
        const nexus::animation::Pose& sourcePose,
        const nexus::animation::Skeleton& sourceSkeleton,
        const nexus::animation::Skeleton& targetSkeleton,
        nexus::animation::Pose& outPose);
};

} // namespace nexus::eval
