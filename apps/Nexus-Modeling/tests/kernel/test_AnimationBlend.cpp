#include <gtest/gtest.h>

#include <nexus/animation/AnimationBlend.h>
#include <nexus/animation/AnimationCore.h>

using namespace nexus::animation;

TEST(AnimationBlend, FactorZeroReturnsPoseA) {
    Skeleton skel;
    (void)skel.addBone({"root", -1, {}});
    (void)skel.addBone({"child", 0, {{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}});

    Pose poseA(2);
    poseA.setLocalTransform(0, {{1, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}});
    poseA.setLocalTransform(1, {{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}});
    Pose poseB(2);
    poseB.setLocalTransform(0, {{0, 2, 0}, {0, 0, 0, 1}, {1, 1, 1}});
    poseB.setLocalTransform(1, {{3, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}});

    Pose result = AnimationBlend::blend(poseA, poseB, 0.f);
    EXPECT_FLOAT_EQ(result.localTransform(0).translation.x, 1.f);
    EXPECT_FLOAT_EQ(result.localTransform(0).translation.y, 0.f);
    EXPECT_FLOAT_EQ(result.localTransform(1).translation.x, 0.f);
    EXPECT_FLOAT_EQ(result.localTransform(1).translation.y, 0.f);
}

TEST(AnimationBlend, FactorOneReturnsPoseB) {
    Skeleton skel;
    (void)skel.addBone({"root", -1, {}});

    Pose poseA(1);
    poseA.setLocalTransform(0, {{1, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}});
    Pose poseB(1);
    poseB.setLocalTransform(0, {{0, 2, 5}, {0, 0, 0, 1}, {1, 1, 1}});

    Pose result = AnimationBlend::blend(poseA, poseB, 1.f);
    EXPECT_FLOAT_EQ(result.localTransform(0).translation.x, 0.f);
    EXPECT_FLOAT_EQ(result.localTransform(0).translation.y, 2.f);
    EXPECT_FLOAT_EQ(result.localTransform(0).translation.z, 5.f);
}

TEST(AnimationBlend, MidpointIsAverage) {
    Skeleton skel;
    (void)skel.addBone({"root", -1, {}});

    Pose poseA(1);
    poseA.setLocalTransform(0, {{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}});
    Pose poseB(1);
    poseB.setLocalTransform(0, {{4, 6, 8}, {0, 0, 0, 1}, {1, 1, 1}});

    Pose result = AnimationBlend::blend(poseA, poseB, 0.5f);
    EXPECT_FLOAT_EQ(result.localTransform(0).translation.x, 2.f);
    EXPECT_FLOAT_EQ(result.localTransform(0).translation.y, 3.f);
    EXPECT_FLOAT_EQ(result.localTransform(0).translation.z, 4.f);
}
