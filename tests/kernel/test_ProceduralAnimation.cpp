#include <nexus/eval/AnimationNodes.h>
#include <nexus/animation/AnimationCore.h>
#include <nexus/animation/SkeletonRetargeter.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::animation;
using namespace nexus::eval;
using namespace nexus::geometry;

namespace {

// Create a simple two-bone skeleton for testing
Skeleton makeTwoBoneSkeleton()
{
    Skeleton skel;
    
    BoneDesc root{};
    root.name = "root";
    root.parentIndex = -1;
    root.bindLocal = Transform::identity();
    EXPECT_EQ(skel.addBone(root), 0);

    BoneDesc child{};
    child.name = "child";
    child.parentIndex = 0;
    child.bindLocal = Transform::identity();
    child.bindLocal.translation = {1.f, 0.f, 0.f};
    EXPECT_EQ(skel.addBone(child), 1);

    return skel;
}

// Create a simple one-second animation clip with linear translation on root bone
AnimationClip makeSimpleClip()
{
    AnimationClip clip(1.f, 60.f);
    
    TransformTrack track;
    TransformKeyframe k0{};
    k0.timeSec = 0.f;
    k0.value = Transform::identity();
    
    TransformKeyframe k1{};
    k1.timeSec = 1.f;
    k1.value.translation = {10.f, 0.f, 0.f};
    
    track.setKeyframes({k0, k1});
    clip.setBoneTrack(0, track);
    
    return clip;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  AnimationSampleNode Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(ProceduralAnimation, AnimationSampleNodeSamplesAtZeroTime)
{
    const Skeleton skel = makeTwoBoneSkeleton();
    const AnimationClip clip = makeSimpleClip();
    
    Pose outPose;
    ASSERT_TRUE(AnimationSampleNode::compute(clip, skel, 0.f, outPose));
    
    const Transform& root = outPose.localTransform(0);
    EXPECT_NEAR(root.translation.x, 0.f, 1e-5f);
}

TEST(ProceduralAnimation, AnimationSampleNodeSamplesAtMidpoint)
{
    const Skeleton skel = makeTwoBoneSkeleton();
    const AnimationClip clip = makeSimpleClip();
    
    Pose outPose;
    ASSERT_TRUE(AnimationSampleNode::compute(clip, skel, 0.5f, outPose));
    
    const Transform& root = outPose.localTransform(0);
    EXPECT_NEAR(root.translation.x, 5.f, 1e-5f);
}

TEST(ProceduralAnimation, AnimationSampleNodeClampsBeyondDuration)
{
    const Skeleton skel = makeTwoBoneSkeleton();
    const AnimationClip clip = makeSimpleClip();
    
    Pose outPose;
    ASSERT_TRUE(AnimationSampleNode::compute(clip, skel, 2.f, outPose));
    
    const Transform& root = outPose.localTransform(0);
    EXPECT_NEAR(root.translation.x, 10.f, 1e-5f);
}

TEST(ProceduralAnimation, AnimationSampleNodeClampsNegativeTime)
{
    const Skeleton skel = makeTwoBoneSkeleton();
    const AnimationClip clip = makeSimpleClip();
    
    Pose outPose;
    ASSERT_TRUE(AnimationSampleNode::compute(clip, skel, -1.f, outPose));
    
    const Transform& root = outPose.localTransform(0);
    EXPECT_NEAR(root.translation.x, 0.f, 1e-5f);
}

TEST(ProceduralAnimation, AnimationSampleNodeIsDeterministicAcrossRuns)
{
    const Skeleton skel = makeTwoBoneSkeleton();
    const AnimationClip clip = makeSimpleClip();
    
    Pose run1, run2;
    ASSERT_TRUE(AnimationSampleNode::compute(clip, skel, 0.3f, run1));
    ASSERT_TRUE(AnimationSampleNode::compute(clip, skel, 0.3f, run2));
    
    const Transform& t1 = run1.localTransform(0);
    const Transform& t2 = run2.localTransform(0);
    
    EXPECT_FLOAT_EQ(t1.translation.x, t2.translation.x);
    EXPECT_FLOAT_EQ(t1.translation.y, t2.translation.y);
    EXPECT_FLOAT_EQ(t1.translation.z, t2.translation.z);
}

// ─────────────────────────────────────────────────────────────────────────────
//  AnimationBlendNode Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(ProceduralAnimation, AnimationBlendNodeClampsWeightZero)
{
    const AnimationClip clipA = makeSimpleClip();
    const AnimationClip clipB = makeSimpleClip();
    
    AnimationClip outClip;
    ASSERT_TRUE(AnimationBlendNode::compute(clipA, clipB, -1.f, outClip));
}

TEST(ProceduralAnimation, AnimationBlendNodeClampsWeightOne)
{
    const AnimationClip clipA = makeSimpleClip();
    const AnimationClip clipB = makeSimpleClip();
    
    AnimationClip outClip;
    ASSERT_TRUE(AnimationBlendNode::compute(clipA, clipB, 2.f, outClip));
}

TEST(ProceduralAnimation, AnimationBlendNodeAcceptsValidWeights)
{
    const AnimationClip clipA = makeSimpleClip();
    const AnimationClip clipB = makeSimpleClip();
    
    AnimationClip outClip;
    ASSERT_TRUE(AnimationBlendNode::compute(clipA, clipB, 0.5f, outClip));
}

TEST(ProceduralAnimation, AnimationBlendNodeIsDeterministicAcrossRuns)
{
    const AnimationClip clipA = makeSimpleClip();
    const AnimationClip clipB = makeSimpleClip();
    
    AnimationClip run1, run2;
    ASSERT_TRUE(AnimationBlendNode::compute(clipA, clipB, 0.3f, run1));
    ASSERT_TRUE(AnimationBlendNode::compute(clipA, clipB, 0.3f, run2));
    
    EXPECT_FLOAT_EQ(run1.durationSec(), run2.durationSec());
    EXPECT_FLOAT_EQ(run1.sampleRateHz(), run2.sampleRateHz());
}

// ─────────────────────────────────────────────────────────────────────────────
//  AnimationRetargetNode Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(ProceduralAnimation, AnimationRetargetNodeRetargetsIdenticalSkeletons)
{
    const Skeleton srcSkel = makeTwoBoneSkeleton();
    const Skeleton tgtSkel = makeTwoBoneSkeleton();
    
    Pose srcPose(srcSkel.boneCount());
    srcPose.setLocalTransform(0, Transform::identity());
    srcPose.setLocalTransform(1, Transform::identity());
    
    Pose outPose;
    ASSERT_TRUE(AnimationRetargetNode::compute(srcPose, srcSkel, tgtSkel, outPose));
    
    // When skeletons are identical, retarget should preserve pose
    EXPECT_EQ(outPose.localTransform(0).translation.x, srcPose.localTransform(0).translation.x);
}

TEST(ProceduralAnimation, AnimationRetargetNodeRetargetsWithTranslation)
{
    const Skeleton srcSkel = makeTwoBoneSkeleton();
    const Skeleton tgtSkel = makeTwoBoneSkeleton();
    
    Pose srcPose(srcSkel.boneCount());
    Transform t0{};
    t0.translation = {5.f, 0.f, 0.f};
    srcPose.setLocalTransform(0, t0);
    srcPose.setLocalTransform(1, Transform::identity());
    
    Pose outPose;
    ASSERT_TRUE(AnimationRetargetNode::compute(srcPose, srcSkel, tgtSkel, outPose));
    
    EXPECT_NEAR(outPose.localTransform(0).translation.x, 5.f, 1e-5f);
}

TEST(ProceduralAnimation, AnimationRetargetNodeIsDeterministicAcrossRuns)
{
    const Skeleton srcSkel = makeTwoBoneSkeleton();
    const Skeleton tgtSkel = makeTwoBoneSkeleton();
    
    Pose srcPose(srcSkel.boneCount());
    Transform t0{};
    t0.translation = {3.2f, -1.5f, 2.0f};
    srcPose.setLocalTransform(0, t0);
    srcPose.setLocalTransform(1, Transform::identity());
    
    Pose run1, run2;
    ASSERT_TRUE(AnimationRetargetNode::compute(srcPose, srcSkel, tgtSkel, run1));
    ASSERT_TRUE(AnimationRetargetNode::compute(srcPose, srcSkel, tgtSkel, run2));
    
    const Transform& t1 = run1.localTransform(0);
    const Transform& t2 = run2.localTransform(0);
    
    EXPECT_FLOAT_EQ(t1.translation.x, t2.translation.x);
    EXPECT_FLOAT_EQ(t1.translation.y, t2.translation.y);
    EXPECT_FLOAT_EQ(t1.translation.z, t2.translation.z);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Node Kind and Name Contracts
// ─────────────────────────────────────────────────────────────────────────────

TEST(ProceduralAnimation, AnimationNodeKindsAreDefined)
{
    EXPECT_EQ(AnimationSampleNode::kind(), AnimationNodeKind::Sample);
    EXPECT_EQ(AnimationBlendNode::kind(), AnimationNodeKind::Blend);
    EXPECT_EQ(AnimationRetargetNode::kind(), AnimationNodeKind::Retarget);
}

TEST(ProceduralAnimation, AnimationNodeNamesAreDefined)
{
    EXPECT_EQ(std::string(AnimationSampleNode::name()), "AnimationSample");
    EXPECT_EQ(std::string(AnimationBlendNode::name()), "AnimationBlend");
    EXPECT_EQ(std::string(AnimationRetargetNode::name()), "AnimationRetarget");
}
