#include <nexus/eval/AnimationNodes.h>

namespace nexus::eval {

bool AnimationBlendNode::compute(
    const nexus::animation::AnimationClip& clipA,
    const nexus::animation::AnimationClip& /* clipB */,
    float weight,
    nexus::animation::AnimationClip& outClip)
{
    // Clamp weight to [0,1] for deterministic output
    // (weight not used in placeholder implementation, but validated for determinism)
    const float w = (weight < 0.f) ? 0.f : (weight > 1.f) ? 1.f : weight;
    (void)w;  // suppress unused variable warning in placeholder

    // Animation blending is typically done at the pose level via ClipBlender,
    // not at the clip level. This is a placeholder that validates the contract.
    // In a real procedural graph, blend would operate on sampled poses.
    outClip = clipA;
    return true;
}

bool AnimationSampleNode::compute(
    const nexus::animation::AnimationClip& clip,
    const nexus::animation::Skeleton& skeleton,
    float timeSeconds,
    nexus::animation::Pose& outPose)
{
    // Clamp time to clip duration for determinism
    const float duration = clip.durationSec();
    const float t = (timeSeconds < 0.f) ? 0.f : (timeSeconds > duration) ? duration : timeSeconds;

    // Sample the clip using the standard deterministic path
    clip.sampleToPose(t, skeleton, outPose);
    return true;
}

bool AnimationRetargetNode::compute(
    const nexus::animation::Pose& sourcePose,
    const nexus::animation::Skeleton& sourceSkeleton,
    const nexus::animation::Skeleton& targetSkeleton,
    nexus::animation::Pose& outPose)
{
    // Build a name-based mapping from source to target skeleton
    const nexus::animation::SkeletonMapping mapping = 
        nexus::animation::SkeletonMapping::buildByName(sourceSkeleton, targetSkeleton);

    // Apply the retarget mapping deterministically
    nexus::animation::SkeletonRetargeter::retarget(
        sourcePose, sourceSkeleton, mapping, targetSkeleton, outPose);
    return true;
}

} // namespace nexus::eval
