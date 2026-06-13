#include <gtest/gtest.h>

#include <nexus/geometry/NurbsCurveClosestPoint.h>
#include <nexus/geometry/NurbsCurve.h>

namespace {

using namespace nexus::geometry;

static NurbsCurve makeLineCurve()
{
    return NurbsCurve(
        1,
        {0.f, 0.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f},
            Vec3{10.f, 0.f, 0.f},
        });
}

TEST(NurbsCurveClosestPoint, PointOnCurveReturnsSame)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {5.f, 0.f, 0.f});
    EXPECT_NEAR(result.distance, 0.f, 1e-5f);
    EXPECT_NEAR(result.param, 0.5f, 1e-4f);
    EXPECT_NEAR(result.point.x, 5.f, 1e-4f);
    EXPECT_NEAR(result.point.y, 0.f, 1e-4f);
}

TEST(NurbsCurveClosestPoint, PointAboveLineReturnsCorrectProjection)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {5.f, 2.f, 0.f});
    EXPECT_NEAR(result.point.x, 5.f, 1e-4f);
    EXPECT_NEAR(result.point.y, 0.f, 1e-4f);
    EXPECT_NEAR(result.distance, 2.f, 1e-3f);
}

TEST(NurbsCurveClosestPoint, PointBeforeStartSnapsToEndpoint)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {-3.f, 0.f, 0.f});
    EXPECT_NEAR(result.param, 0.f, 1e-3f);
    EXPECT_NEAR(result.point.x, 0.f, 1e-3f);
}

TEST(NurbsCurveClosestPoint, PointAfterEndSnapsToEndpoint)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {13.f, 0.f, 0.f});
    EXPECT_NEAR(result.param, 1.f, 1e-3f);
    EXPECT_NEAR(result.point.x, 10.f, 1e-3f);
}

TEST(NurbsCurveClosestPoint, WorksWithDefaultParameters)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {7.f, 1.f, 0.f});
    EXPECT_TRUE(result.converged);
    EXPECT_GT(result.distance, 0.f);
    EXPECT_TRUE(std::isfinite(result.param));
    EXPECT_TRUE(std::isfinite(result.point.x));
}

} // namespace
