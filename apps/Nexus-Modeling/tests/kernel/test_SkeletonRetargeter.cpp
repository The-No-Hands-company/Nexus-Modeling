#include <nexus/animation/SkeletonRetargeter.h>

#include <gtest/gtest.h>

using namespace nexus::animation;

// ── SkeletonMapping buildByName ──────────────────────────────────────────────

TEST(SkeletonRetargeter, BuildByNameMapsSameNameBones)
{
    Skeleton src;
    ASSERT_EQ(src.addBone({"root", -1, {}}), 0);
    ASSERT_EQ(src.addBone({"upper", 0, {}}), 1);
    ASSERT_EQ(src.addBone({"lower", 1, {}}), 2);

    Skeleton tgt;
    ASSERT_EQ(tgt.addBone({"root", -1, {}}), 0);
    ASSERT_EQ(tgt.addBone({"upper", 0, {}}), 1);
    ASSERT_EQ(tgt.addBone({"lower", 1, {}}), 2);

    const SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);
    EXPECT_EQ(m.mappingCount(), 3u);
    EXPECT_EQ(m.targetBoneFor(0), 0);
    EXPECT_EQ(m.targetBoneFor(1), 1);
    EXPECT_EQ(m.targetBoneFor(2), 2);
}

TEST(SkeletonRetargeter, BuildByNameSkipsUnmatchedBones)
{
    Skeleton src;
    ASSERT_EQ(src.addBone({"root", -1, {}}), 0);
    ASSERT_EQ(src.addBone({"extra", 0, {}}), 1);

    Skeleton tgt;
    ASSERT_EQ(tgt.addBone({"root", -1, {}}), 0);

    const SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);
    EXPECT_EQ(m.mappingCount(), 1u);
    EXPECT_EQ(m.targetBoneFor(0), 0);
    EXPECT_EQ(m.targetBoneFor(1), Skeleton::kInvalidBone);
}

TEST(SkeletonRetargeter, BuildByNameEmptySourceProducesEmptyMapping)
{
    Skeleton src;
    Skeleton tgt;
    ASSERT_EQ(tgt.addBone({"root", -1, {}}), 0);

    const SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);
    EXPECT_EQ(m.mappingCount(), 0u);
}

// ── SkeletonMapping mapBone ─────────────────────────────────────────────────

TEST(SkeletonRetargeter, MapBoneMapsByNamePair)
{
    Skeleton tgt;
    ASSERT_EQ(tgt.addBone({"targetRoot", -1, {}}), 0);

    SkeletonMapping m;
    EXPECT_TRUE(m.mapBone("sourceRoot", "targetRoot", tgt));
    EXPECT_EQ(m.targetBoneForSourceName("sourceRoot"), 0);
}

TEST(SkeletonRetargeter, MapBoneFailsForUnknownTargetBoneName)
{
    Skeleton tgt;
    ASSERT_EQ(tgt.addBone({"onlyBone", -1, {}}), 0);

    SkeletonMapping m;
    EXPECT_FALSE(m.mapBone("src", "nonexistent", tgt));
}

// ── SkeletonMapping query ────────────────────────────────────────────────────

TEST(SkeletonRetargeter, TargetBoneForReturnsInvalidForUnmapped)
{
    SkeletonMapping m;
    EXPECT_EQ(m.targetBoneFor(0), Skeleton::kInvalidBone);
    EXPECT_EQ(m.targetBoneFor(42), Skeleton::kInvalidBone);
}

TEST(SkeletonRetargeter, TargetBoneForSourceNameReturnsInvalidForUnmapped)
{
    SkeletonMapping m;
    EXPECT_EQ(m.targetBoneForSourceName("missing"), Skeleton::kInvalidBone);
}

TEST(SkeletonRetargeter, ClearRemovesAllMappings)
{
    Skeleton src;
    ASSERT_EQ(src.addBone({"root", -1, {}}), 0);
    Skeleton tgt;
    ASSERT_EQ(tgt.addBone({"root", -1, {}}), 0);

    SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);
    EXPECT_EQ(m.mappingCount(), 1u);

    m.clear();
    EXPECT_EQ(m.mappingCount(), 0u);
    EXPECT_EQ(m.targetBoneFor(0), Skeleton::kInvalidBone);
}

// ── SkeletonRetargeter::retarget ──────────────────────────────────────────────

TEST(SkeletonRetargeter, RetargetCopiesSourcePoseForMappedBones)
{
    Skeleton src;
    ASSERT_EQ(src.addBone({"root", -1, {}}), 0);

    Skeleton tgt;
    ASSERT_EQ(tgt.addBone({"root", -1, {}}), 0);

    const SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);

    Pose srcPose(src.boneCount());
    Transform st{};
    st.translation = {5.f, 0.f, 0.f};
    srcPose.setLocalTransform(0, st);

    Pose out;
    SkeletonRetargeter::retarget(srcPose, src, m, tgt, out);

    EXPECT_FLOAT_EQ(out.localTransform(0).translation.x, 5.f);
}

TEST(SkeletonRetargeter, RetargetUnmappedBoneFallsBackToBindLocal)
{
    Skeleton src;
    ASSERT_EQ(src.addBone({"srcOnly", -1, {}}), 0);

    Skeleton tgt;
    Transform bindT{};
    bindT.translation = {99.f, 0.f, 0.f};
    ASSERT_EQ(tgt.addBone({"tgtOnly", -1, bindT}), 0);

    const SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);

    Pose srcPose(src.boneCount());
    Pose out;
    SkeletonRetargeter::retarget(srcPose, src, m, tgt, out);

    // Unmapped: target bone should hold its bind-local transform.
    EXPECT_FLOAT_EQ(out.localTransform(0).translation.x, 99.f);
}

TEST(SkeletonRetargeter, RetargetEmptySourcePoseFillsAllBonesWithBindLocal)
{
    Skeleton src;
    Skeleton tgt;
    Transform bindT{};
    bindT.translation = {42.f, 0.f, 0.f};
    ASSERT_EQ(tgt.addBone({"only", -1, bindT}), 0);

    const SkeletonMapping m = SkeletonMapping::buildByName(src, tgt);

    Pose srcPose;
    Pose out;
    SkeletonRetargeter::retarget(srcPose, src, m, tgt, out);

    EXPECT_FLOAT_EQ(out.localTransform(0).translation.x, 42.f);
}

TEST(SkeletonRetargeter, RetargetResizesOutputToTargetBoneCount)
{
    Skeleton src;
    Skeleton tgt;
    ASSERT_EQ(tgt.addBone({"a", -1, {}}), 0);
    ASSERT_EQ(tgt.addBone({"b", 0, {}}), 1);

    const SkeletonMapping m;

    Pose srcPose;
    Pose out(10u);
    SkeletonRetargeter::retarget(srcPose, src, m, tgt, out);
    out.computeModelMatrices(tgt);
    EXPECT_EQ(out.modelMatrices().size(), 2u);
}
