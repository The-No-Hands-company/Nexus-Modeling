#include <gtest/gtest.h>

#include <nexus/geometry/NurbsSurfaceSplit.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static NurbsSurface makeFlatSurface()
{
    return NurbsSurface(
        2, 2,
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f}, Vec3{0.f, 2.f, 0.f},
            Vec3{1.f, 0.f, 0.f}, Vec3{1.f, 1.f, 0.f}, Vec3{1.f, 2.f, 0.f},
            Vec3{2.f, 0.f, 0.f}, Vec3{2.f, 1.f, 0.f}, Vec3{2.f, 2.f, 0.f},
        },
        3, 3);
}

TEST(NurbsSurfaceSplit, SplitUAtHalfWorks)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    auto result = NurbsSurfaceSplit::split(surface, 0.5f, SplitDirection::U);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.left.isValid());
    EXPECT_TRUE(result.right.isValid());

    EXPECT_GT(result.left.controlPointCountU(), 0);
    EXPECT_GT(result.right.controlPointCountU(), 0);
}

TEST(NurbsSurfaceSplit, SplitVAtHalfWorks)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    auto result = NurbsSurfaceSplit::split(surface, 0.5f, SplitDirection::V);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.left.isValid());
    EXPECT_TRUE(result.right.isValid());

    EXPECT_GT(result.left.controlPointCountV(), 0);
    EXPECT_GT(result.right.controlPointCountV(), 0);
}

TEST(NurbsSurfaceSplit, SplitAt0Fails)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    auto result = NurbsSurfaceSplit::split(surface, 0.f, SplitDirection::U);
    EXPECT_FALSE(result.valid);
}

TEST(NurbsSurfaceSplit, SplitAt1Fails)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    auto result = NurbsSurfaceSplit::split(surface, 1.f, SplitDirection::U);
    EXPECT_FALSE(result.valid);
}

TEST(NurbsSurfaceSplit, EmptySurfaceFails)
{
    NurbsSurface empty;
    EXPECT_FALSE(empty.isValid());

    auto result = NurbsSurfaceSplit::split(empty, 0.5f, SplitDirection::U);
    EXPECT_FALSE(result.valid);
}

TEST(NurbsSurfaceSplit, SplitAtQuarterUWorks)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    auto result = NurbsSurfaceSplit::split(surface, 0.25f, SplitDirection::U);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.left.isValid());
    EXPECT_TRUE(result.right.isValid());
}

TEST(NurbsSurfaceSplit, SplitAtThreeQuarterVWorks)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    auto result = NurbsSurfaceSplit::split(surface, 0.75f, SplitDirection::V);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.left.isValid());
    EXPECT_TRUE(result.right.isValid());
}

} // namespace
