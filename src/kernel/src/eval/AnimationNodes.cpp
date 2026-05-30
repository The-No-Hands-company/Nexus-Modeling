#include <nexus/eval/AnimationNodes.h>

#include <cmath>

namespace nexus::eval {

bool AnimationBlendNode::compute(
    const nexus::animation::AnimationClip& clipA,
    const nexus::animation::AnimationClip& clipB,
    float weight,
    nexus::animation::AnimationClip& outClip)
{
    const float w = (weight < 0.f) ? 0.f : (weight > 1.f) ? 1.f : weight;

    const float durationA   = clipA.durationSec();
    const float durationB   = clipB.durationSec();
    const float outDuration = durationA + w * (durationB - durationA);
    // Prefer clipA's sample rate; fall back to clipB's; default to 60 Hz.
    const float sampleRate  = clipA.sampleRateHz() > 0.f ? clipA.sampleRateHz()
                            : clipB.sampleRateHz() > 0.f ? clipB.sampleRateHz()
                            : 60.f;

    outClip = nexus::animation::AnimationClip(outDuration, sampleRate);
    if (outDuration <= 0.f) return true;

    // Bake one keyframe per sample interval for every bone slot present in
    // either clip.  Bones absent from a clip fall back to identity.
    const size_t nSlots = std::max(clipA.trackSlotCount(), clipB.trackSlotCount());
    const int    nSteps = static_cast<int>(std::ceil(outDuration * sampleRate)) + 1;
    const float  dt     = outDuration / static_cast<float>(nSteps > 1 ? nSteps - 1 : 1);

    for (size_t bone = 0; bone < nSlots; ++bone) {
        const bool hasA = clipA.hasBoneTrack(bone);
        const bool hasB = clipB.hasBoneTrack(bone);
        if (!hasA && !hasB) continue;

        std::vector<nexus::animation::TransformKeyframe> keys;
        keys.reserve(static_cast<size_t>(nSteps));

        for (int step = 0; step < nSteps; ++step) {
            const float t  = static_cast<float>(step) * dt;
            // Map output time into each clip's own timeline proportionally.
            const float tA = (durationA > 0.f) ? t * (durationA / outDuration) : 0.f;
            const float tB = (durationB > 0.f) ? t * (durationB / outDuration) : 0.f;

            const nexus::animation::Transform xfA = hasA
                ? clipA.boneTrack(bone)->sample(tA)
                : nexus::animation::Transform::identity();
            const nexus::animation::Transform xfB = hasB
                ? clipB.boneTrack(bone)->sample(tB)
                : nexus::animation::Transform::identity();

            nexus::animation::Transform blended;
            // Translation and scale: linear lerp.
            blended.translation.x = xfA.translation.x + w * (xfB.translation.x - xfA.translation.x);
            blended.translation.y = xfA.translation.y + w * (xfB.translation.y - xfA.translation.y);
            blended.translation.z = xfA.translation.z + w * (xfB.translation.z - xfA.translation.z);
            blended.scale.x = xfA.scale.x + w * (xfB.scale.x - xfA.scale.x);
            blended.scale.y = xfA.scale.y + w * (xfB.scale.y - xfA.scale.y);
            blended.scale.z = xfA.scale.z + w * (xfB.scale.z - xfA.scale.z);
            // Rotation: nlerp — deterministic, no trig.
            blended.rotation.x = xfA.rotation.x + w * (xfB.rotation.x - xfA.rotation.x);
            blended.rotation.y = xfA.rotation.y + w * (xfB.rotation.y - xfA.rotation.y);
            blended.rotation.z = xfA.rotation.z + w * (xfB.rotation.z - xfA.rotation.z);
            blended.rotation.w = xfA.rotation.w + w * (xfB.rotation.w - xfA.rotation.w);
            const float qLen = std::sqrt(
                blended.rotation.x * blended.rotation.x +
                blended.rotation.y * blended.rotation.y +
                blended.rotation.z * blended.rotation.z +
                blended.rotation.w * blended.rotation.w);
            if (qLen > 1e-6f) {
                const float inv = 1.f / qLen;
                blended.rotation.x *= inv;
                blended.rotation.y *= inv;
                blended.rotation.z *= inv;
                blended.rotation.w *= inv;
            } else {
                blended.rotation = {0.f, 0.f, 0.f, 1.f};
            }

            nexus::animation::TransformKeyframe kf;
            kf.timeSec = t;
            kf.value   = blended;
            keys.push_back(kf);
        }

        nexus::animation::TransformTrack track;
        track.setKeyframes(std::move(keys));
        outClip.setBoneTrack(bone, std::move(track));
    }

    return true;
}

bool AnimationSampleNode::compute(
    const nexus::animation::AnimationClip& clip,
    const nexus::animation::Skeleton& skeleton,
    float timeSeconds,
    nexus::animation::Pose& outPose)
{
    const float duration = clip.durationSec();
    const float t = (timeSeconds < 0.f) ? 0.f : (timeSeconds > duration) ? duration : timeSeconds;
    clip.sampleToPose(t, skeleton, outPose);
    return true;
}

bool AnimationRetargetNode::compute(
    const nexus::animation::Pose& sourcePose,
    const nexus::animation::Skeleton& sourceSkeleton,
    const nexus::animation::Skeleton& targetSkeleton,
    nexus::animation::Pose& outPose)
{
    const nexus::animation::SkeletonMapping mapping =
        nexus::animation::SkeletonMapping::buildByName(sourceSkeleton, targetSkeleton);
    nexus::animation::SkeletonRetargeter::retarget(
        sourcePose, sourceSkeleton, mapping, targetSkeleton, outPose);
    return true;
}

} // namespace nexus::eval
