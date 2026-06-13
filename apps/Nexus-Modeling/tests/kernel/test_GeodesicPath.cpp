#include <gtest/gtest.h>

#include <nexus/geometry/GeodesicPath.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cmath>
#include <vector>

namespace {

using namespace nexus::geometry;

static NurbsSurface makePlanarPatch()
{
    const std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    const std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    const std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {2.f, 0.f, 0.f}, {3.f, 0.f, 0.f},
        {0.f, 1.f, 0.f}, {1.f, 1.f, 0.f}, {2.f, 1.f, 0.f}, {3.f, 1.f, 0.f},
        {0.f, 2.f, 0.f}, {1.f, 2.f, 0.f}, {2.f, 2.f, 0.f}, {3.f, 2.f, 0.f},
        {0.f, 3.f, 0.f}, {1.f, 3.f, 0.f}, {2.f, 3.f, 0.f}, {3.f, 3.f, 0.f},
    };
    return NurbsSurface(3, 3, kU, kV, ctl, 4, 4);
}

TEST(GeodesicPath, StraightLineOnPlaneHasExpectedArcLength)
{
    auto surface = makePlanarPatch();
    ASSERT_TRUE(surface.isValid());

    constexpr float expected = std::sqrt(2.0f);

    const auto result = GeodesicPath::compute(surface, 0.f, 0.f, 1.f / 3.f, 1.f / 3.f);

    ASSERT_FALSE(result.path3D.empty());
    EXPECT_NEAR(result.arcLength, expected, 1e-2f);
    EXPECT_GT(result.path3D.size(), 1u);
}

TEST(GeodesicPath, SameEndpointReturnsZeroLength)
{
    auto surface = makePlanarPatch();
    ASSERT_TRUE(surface.isValid());

    const auto result = GeodesicPath::compute(surface, 0.5f, 0.5f, 0.5f, 0.5f);

    EXPECT_NEAR(result.arcLength, 0.f, 1e-4f);
    EXPECT_FALSE(result.path3D.empty());
}

TEST(GeodesicPath, EmptySurfaceFails)
{
    NurbsSurface empty;
    EXPECT_FALSE(empty.isValid());

    const auto result = GeodesicPath::compute(empty, 0.f, 0.f, 1.f, 1.f);

    EXPECT_EQ(result.arcLength, 0.f);
}

} // namespace
