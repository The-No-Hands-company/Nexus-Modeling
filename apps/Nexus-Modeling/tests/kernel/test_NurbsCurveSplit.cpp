#include <gtest/gtest.h>

#include <nexus/geometry/NurbsCurveSplit.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static NurbsCurve makeQuadraticCurve()
{
    return NurbsCurve(
        2,
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f},
            Vec3{5.f, 2.f, 0.f},
            Vec3{10.f, 0.f, 0.f},
        });
}

TEST(NurbsCurveSplit, SplitAtHalfProducesTwoCurves)
{
    auto curve = makeQuadraticCurve();
    ASSERT_TRUE(curve.isValid());

    auto result = NurbsCurveSplit::split(curve, 0.5f);
    ASSERT_TRUE(result.valid);
    ASSERT_TRUE(result.left.isValid());

    EXPECT_GT(result.left.controlPointCount(), 0);
    EXPECT_GT(result.right.controlPointCount(), 0);
}

TEST(NurbsCurveSplit, JoinPointMatches)
{
    auto curve = makeQuadraticCurve();
    ASSERT_TRUE(curve.isValid());

    auto result = NurbsCurveSplit::split(curve, 0.5f);
    ASSERT_TRUE(result.valid);
    ASSERT_TRUE(result.left.isValid());

    auto dom = result.left.domain();
    float t = std::nextafter(dom.second, dom.first); // evaluate just inside clamped endpoint
    Vec3 leftEnd  = result.left.evaluate(t);
    Vec3 midExpected = curve.evaluate(0.5f);
    EXPECT_NEAR(leftEnd.x, midExpected.x, 1e-3f);
    EXPECT_NEAR(leftEnd.y, midExpected.y, 1e-3f);
}

TEST(NurbsCurveSplit, OutOfRangeFailsAtZero)
{
    auto curve = makeQuadraticCurve();
    ASSERT_TRUE(curve.isValid());

    auto result = NurbsCurveSplit::split(curve, 0.f);
    EXPECT_FALSE(result.valid);
}

TEST(NurbsCurveSplit, OutOfRangeFailsAtOne)
{
    auto curve = makeQuadraticCurve();
    ASSERT_TRUE(curve.isValid());

    auto result = NurbsCurveSplit::split(curve, 1.f);
    EXPECT_FALSE(result.valid);
}

TEST(NurbsCurveSplit, QuadraticCurveSplitWorks)
{
    auto curve = makeQuadraticCurve();
    ASSERT_TRUE(curve.isValid());

    auto result = NurbsCurveSplit::split(curve, 0.5f);
    ASSERT_TRUE(result.valid);
    ASSERT_TRUE(result.left.isValid());
    EXPECT_GT(result.right.controlPointCount(), 0);
}

TEST(NurbsCurveSplit, SplitAtOnePointZeroFails)
{
    auto curve = makeQuadraticCurve();
    ASSERT_TRUE(curve.isValid());

    auto result = NurbsCurveSplit::split(curve, 1.f);
    EXPECT_FALSE(result.valid);
}

TEST(NurbsCurveSplit, SplitNearEndpointWorks)
{
    auto curve = makeQuadraticCurve();
    ASSERT_TRUE(curve.isValid());

    auto result = NurbsCurveSplit::split(curve, 0.02f);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.left.isValid());
    EXPECT_GT(result.right.controlPointCount(), 0);
}

} // namespace
