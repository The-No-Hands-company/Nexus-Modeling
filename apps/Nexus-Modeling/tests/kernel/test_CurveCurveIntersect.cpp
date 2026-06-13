#include <gtest/gtest.h>

#include <nexus/geometry/CurveCurveIntersect.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static NurbsCurve makeDiagonalLine()
{
    return NurbsCurve(
        1,
        {0.f, 0.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f},
            Vec3{1.f, 1.f, 0.f},
        });
}

static NurbsCurve makeAntiDiagonalLine()
{
    return NurbsCurve(
        1,
        {0.f, 0.f, 1.f, 1.f},
        {
            Vec3{1.f, 0.f, 0.f},
            Vec3{0.f, 1.f, 0.f},
        });
}

static NurbsCurve makeHorizontalLine(float y)
{
    return NurbsCurve(
        1,
        {0.f, 0.f, 1.f, 1.f},
        {
            Vec3{0.f, y, 0.f},
            Vec3{1.f, y, 0.f},
        });
}

TEST(CurveCurveIntersect, CrossingLinesDetected)
{
    auto a = makeDiagonalLine();
    auto b = makeAntiDiagonalLine();

    auto results = CurveCurveIntersect::intersect(a, b, 65, 1e-3f);

    ASSERT_GE(results.size(), 1u);

    bool foundHalf = false;
    for (const auto& inter : results) {
        if (std::abs(inter.point.x - 0.5f) < 0.01f &&
            std::abs(inter.point.y - 0.5f) < 0.01f) {
            foundHalf = true;
            break;
        }
    }
    EXPECT_TRUE(foundHalf);
}

TEST(CurveCurveIntersect, ParallelLinesReturnNoIntersections)
{
    auto a = makeHorizontalLine(0.f);
    auto b = makeHorizontalLine(1.f);

    auto results = CurveCurveIntersect::intersect(a, b);
    EXPECT_EQ(results.size(), 0u);
}

TEST(CurveCurveIntersect, SameLineReturnsIntersections)
{
    auto a = makeDiagonalLine();
    auto b = makeDiagonalLine();

    auto results = CurveCurveIntersect::intersect(a, b);
    EXPECT_GT(results.size(), 0u);

    for (const auto& inter : results) {
        EXPECT_NEAR(inter.point.x, inter.point.y, 1e-3f);
    }
}

} // namespace
