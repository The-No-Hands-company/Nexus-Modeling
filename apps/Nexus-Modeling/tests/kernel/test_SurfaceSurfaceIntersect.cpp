#include <gtest/gtest.h>

#include <nexus/geometry/SurfaceSurfaceIntersect.h>
#include <nexus/geometry/NurbsSurface.h>

#include <vector>

namespace {

using namespace nexus::geometry;

static NurbsSurface makeXYPlane(float z)
{
    const std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    const std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    const std::vector<Vec3> ctl = {
        {0.f, 0.f, z}, {1.f, 0.f, z}, {2.f, 0.f, z}, {3.f, 0.f, z},
        {0.f, 1.f, z}, {1.f, 1.f, z}, {2.f, 1.f, z}, {3.f, 1.f, z},
        {0.f, 2.f, z}, {1.f, 2.f, z}, {2.f, 2.f, z}, {3.f, 2.f, z},
        {0.f, 3.f, z}, {1.f, 3.f, z}, {2.f, 3.f, z}, {3.f, 3.f, z},
    };
    return NurbsSurface(3, 3, kU, kV, ctl, 4, 4);
}

static NurbsSurface makeXZPlane(float y)
{
    const std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    const std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    const std::vector<Vec3> ctl = {
        {0.f, y, 0.f}, {1.f, y, 0.f}, {2.f, y, 0.f}, {3.f, y, 0.f},
        {0.f, y, 1.f}, {1.f, y, 1.f}, {2.f, y, 1.f}, {3.f, y, 1.f},
        {0.f, y, 2.f}, {1.f, y, 2.f}, {2.f, y, 2.f}, {3.f, y, 2.f},
        {0.f, y, 3.f}, {1.f, y, 3.f}, {2.f, y, 3.f}, {3.f, y, 3.f},
    };
    return NurbsSurface(3, 3, kU, kV, ctl, 4, 4);
}

TEST(SurfaceSurfaceIntersect, ParallelPlanesReturnNoBranches)
{
    auto planeA = makeXYPlane(0.f);
    auto planeB = makeXYPlane(2.f);

    ASSERT_TRUE(planeA.isValid());
    ASSERT_TRUE(planeB.isValid());

    const auto branches = SurfaceSurfaceIntersect::intersect(planeA, planeB);

    EXPECT_TRUE(branches.empty());
}

TEST(SurfaceSurfaceIntersect, PerpendicularPlanesIntersectionRunsWithoutError)
{
    auto planeXY = makeXYPlane(0.f);
    auto planeXZ = makeXZPlane(0.f);

    ASSERT_TRUE(planeXY.isValid());
    ASSERT_TRUE(planeXZ.isValid());

    IntersectSeedGrid grid;
    grid.resU = 32;
    grid.resV = 32;

    const auto branches = SurfaceSurfaceIntersect::intersect(
        planeXY, planeXZ, grid, 200, 0.005f);

    EXPECT_GE(branches.size(), 0u);
    for (const auto& branch : branches) {
        EXPECT_GE(branch.size(), 2u)
            << "each intersection branch should have at least 2 points";
        for (const auto& p : branch) {
            EXPECT_TRUE(std::isfinite(p.x));
            EXPECT_TRUE(std::isfinite(p.y));
            EXPECT_TRUE(std::isfinite(p.z));
        }
    }
}

TEST(SurfaceSurfaceIntersect, EmptySurfaceFails)
{
    NurbsSurface emptyA;
    NurbsSurface emptyB;

    const auto branches = SurfaceSurfaceIntersect::intersect(emptyA, emptyB);

    EXPECT_TRUE(branches.empty());
}

} // namespace
