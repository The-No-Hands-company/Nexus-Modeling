#include <gtest/gtest.h>

#include <nexus/geometry/NurbsSurfaceClosestPoint.h>

#include <cmath>
#include <limits>

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

TEST(NurbsSurfaceClosestPoint, PointOnSurfaceReturnsItself)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    Vec3 onSurf = surface.evaluate(0.5f, 0.5f);
    auto result = NurbsSurfaceClosestPoint::project(surface, onSurf);

    EXPECT_NEAR(result.distance, 0.f, 1e-3f);
    EXPECT_NEAR(result.point.x, onSurf.x, 1e-3f);
    EXPECT_NEAR(result.point.y, onSurf.y, 1e-3f);
    EXPECT_NEAR(result.point.z, onSurf.z, 1e-3f);
}

TEST(NurbsSurfaceClosestPoint, PointAbovePlaneReturnsCorrectProjection)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    Vec3 query{1.f, 1.f, 3.f};
    auto result = NurbsSurfaceClosestPoint::project(surface, query);

    EXPECT_NEAR(result.point.z, 0.f, 1e-3f);
    EXPECT_NEAR(result.distance, 3.f, 1e-2f);
    EXPECT_TRUE(result.converged);
}

TEST(NurbsSurfaceClosestPoint, EmptySurfaceFails)
{
    NurbsSurface empty;
    EXPECT_FALSE(empty.isValid());

    auto result = NurbsSurfaceClosestPoint::project(empty, {0.f, 0.f, 0.f});
    EXPECT_FALSE(result.converged);
    EXPECT_FLOAT_EQ(result.distance, 0.f);
}

TEST(NurbsSurfaceClosestPoint, NonFiniteQueryFails)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    float nan = std::numeric_limits<float>::quiet_NaN();
    Vec3 query{nan, 0.f, 0.f};
    auto result = NurbsSurfaceClosestPoint::project(surface, query);

    EXPECT_TRUE(std::isnan(result.distance) || result.distance > 1e6f || !result.converged);
}

TEST(NurbsSurfaceClosestPoint, ConvergesToCornerForFarAwayPoint)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    Vec3 farPoint{0.f, 0.f, 100.f};
    auto result = NurbsSurfaceClosestPoint::project(surface, farPoint);

    EXPECT_NEAR(result.point.x, 0.f, 1e-4f);
    EXPECT_NEAR(result.point.y, 0.f, 1e-4f);
    EXPECT_NEAR(result.point.z, 0.f, 1e-4f);
}

TEST(NurbsSurfaceClosestPoint, DefaultOptionsWork)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    NurbsSurfaceClosestPointOptions opts;
    Vec3 query{1.f, 1.f, 5.f};
    auto result = NurbsSurfaceClosestPoint::project(surface, query, opts);

    EXPECT_GT(result.iterations, 0);
    EXPECT_TRUE(result.converged);
    EXPECT_NEAR(result.point.z, 0.f, 1e-3f);
}

} // namespace
