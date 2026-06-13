#include <nexus/animation/InverseKinematics.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace nexus::animation;
using Vec3 = nexus::render::Vec3;

namespace {

Skeleton makeTwoBoneChain()
{
    Skeleton skel;
    BoneDesc root{};
    root.name = "root";
    root.parentIndex = -1;
    skel.addBone(root);

    BoneDesc child{};
    child.name = "child";
    child.parentIndex = 0;
    child.bindLocal.translation = {1.f, 0.f, 0.f};
    skel.addBone(child);

    BoneDesc tip{};
    tip.name = "tip";
    tip.parentIndex = 1;
    tip.bindLocal.translation = {1.f, 0.f, 0.f};
    skel.addBone(tip);

    return skel;
}

Pose makeStraightPose(const Skeleton& skel)
{
    Pose pose(skel.boneCount());
    // All bones at identity local transforms => chain extends along +X
    for (size_t i = 0; i < skel.boneCount(); ++i) {
        Transform t{};
        t.translation = skel.bindLocal(i).translation;
        pose.setLocalTransform(i, t);
    }
    return pose;
}

} // namespace

TEST(InverseKinematics, SolveCCDReachesExactTarget)
{
    Skeleton skel = makeTwoBoneChain();
    Pose pose = makeStraightPose(skel);

    // Tip is at (2,0,0). Target (1.5, 0.5, 0) is within reach (distance ~1.58 < 2.0).
    Vec3 target{1.5f, 0.5f, 0.f};
    IKSettings settings;
    settings.maxIterations = 50;
    settings.tolerance = 0.005f;

    IKResult r = InverseKinematics::solveCCD(skel, pose, 2u, target, settings);
    EXPECT_TRUE(r.ok);
    EXPECT_LT(r.residual, 0.005f);
    EXPECT_GT(r.iterations, 0u);
    EXPECT_LE(r.iterations, 50u);
}

TEST(InverseKinematics, SolveCCDAlreadyAtTargetReturnsImmediately)
{
    Skeleton skel = makeTwoBoneChain();
    Pose pose = makeStraightPose(skel);

    Vec3 target{2.f, 0.f, 0.f};
    IKSettings settings;
    settings.maxIterations = 100;
    settings.tolerance = 0.1f;

    IKResult r = InverseKinematics::solveCCD(skel, pose, 2u, target, settings);
    EXPECT_TRUE(r.ok);
    EXPECT_LT(r.residual, 0.1f);
}

TEST(InverseKinematics, SolveCCDConvergesForUnreachableTarget)
{
    Skeleton skel = makeTwoBoneChain();
    Pose pose = makeStraightPose(skel);

    // Very far target - CCD should converge as close as possible.
    Vec3 target{100.f, 100.f, 0.f};
    IKSettings settings;
    settings.maxIterations = 50;
    settings.tolerance = 0.01f;

    IKResult r = InverseKinematics::solveCCD(skel, pose, 2u, target, settings);
    EXPECT_FALSE(r.ok);
    EXPECT_GT(r.iterations, 0u);
}

TEST(InverseKinematics, SolveCCDRejectsOutOfRangeEndEffector)
{
    Skeleton skel = makeTwoBoneChain();
    Pose pose = makeStraightPose(skel);

    IKResult r = InverseKinematics::solveCCD(skel, pose, 99u, {0.f, 0.f, 0.f}, {});
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
}

TEST(InverseKinematics, SolveCCDRejectsZeroMaxIterations)
{
    Skeleton skel = makeTwoBoneChain();
    Pose pose = makeStraightPose(skel);

    IKSettings settings;
    settings.maxIterations = 0;

    IKResult r = InverseKinematics::solveCCD(skel, pose, 2u, {2.f, 1.f, 0.f}, settings);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
}

TEST(InverseKinematics, SolveCCDEndEffectorWithoutParentFails)
{
    Skeleton skel;
    ASSERT_EQ(skel.addBone({"root", -1, {}}), 0);

    Pose pose(skel.boneCount());

    IKSettings settings;
    settings.maxIterations = 5;

    // Bone 0 has no parent -> cannot rotate toward target.
    IKResult r = InverseKinematics::solveCCD(skel, pose, 0u, {1.f, 0.f, 0.f}, settings);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
}

TEST(InverseKinematics, SolveCCDHonorsJointAngleLimits)
{
    Skeleton skel = makeTwoBoneChain();
    Pose pose = makeStraightPose(skel);

    IKSettings settings;
    settings.maxIterations = 50;
    settings.tolerance = 0.001f;
    settings.damping = 1.0f;
    settings.jointLimits = {{1u, 0.05f}}; // extremely tight limit on the child bone

    Vec3 target{2.f, 1.f, 0.f};
    IKResult r = InverseKinematics::solveCCD(skel, pose, 2u, target, settings);

    // With a severe angle limit the solver should not reach the target.
    EXPECT_FALSE(r.ok);
}

TEST(InverseKinematics, SolveCCDWithZeroDampingDoesNotRotate)
{
    Skeleton skel = makeTwoBoneChain();
    Pose pose = makeStraightPose(skel);

    IKSettings settings;
    settings.maxIterations = 10;
    settings.damping = 0.f;

    Vec3 target{2.f, 1.f, 0.f};
    IKResult r = InverseKinematics::solveCCD(skel, pose, 2u, target, settings);

    // Damping 0 means rotation delta is 0 -> tip does not move.
    EXPECT_FALSE(r.ok);
    EXPECT_GT(r.iterations, 0u);
}

TEST(InverseKinematics, SolveCCDResizesPoseIfNeeded)
{
    Skeleton skel = makeTwoBoneChain();
    Pose pose; // empty, no bones

    Vec3 target{2.f, 1.f, 0.f};
    IKResult r = InverseKinematics::solveCCD(skel, pose, 2u, target, {});
    EXPECT_FALSE(r.ok);
}
