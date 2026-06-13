#include <gtest/gtest.h>

#include <nexus/geometry/CurveFairing.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static NurbsCurve makeStraightLineCurve()
{
    return NurbsCurve(
        2,
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f},
            Vec3{1.f, 0.f, 0.f},
            Vec3{2.f, 0.f, 0.f},
        });
}

static NurbsCurve makeStraightLine4Points()
{
    return NurbsCurve(
        3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f},
            Vec3{1.f, 0.f, 0.f},
            Vec3{2.f, 0.f, 0.f},
            Vec3{3.f, 0.f, 0.f},
        });
}

static NurbsCurve makeNoisyCurve()
{
    return NurbsCurve(
        3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f},
            Vec3{1.f, 2.f, 0.f},
            Vec3{2.f, -1.f, 0.f},
            Vec3{3.f, 0.f, 0.f},
        });
}

static float maxAbsY(const NurbsCurve& curve)
{
    float maxY = 0.f;
    for (const auto& p : curve.controlPoints()) {
        maxY = std::max(maxY, std::abs(p.y));
    }
    return maxY;
}

TEST(CurveFairing, StraightLineStaysStraight)
{
    auto curve = makeStraightLine4Points();
    ASSERT_TRUE(curve.isValid());

    auto faired = CurveFairing::fair(curve);
    ASSERT_TRUE(faired.isValid());

    float maxY = maxAbsY(faired);
    EXPECT_NEAR(maxY, 0.f, 1e-5f);
}

TEST(CurveFairing, NoisyCurveGetsSmoothed)
{
    auto curve = makeNoisyCurve();
    ASSERT_TRUE(curve.isValid());

    float originalMaxY = maxAbsY(curve);
    EXPECT_GT(originalMaxY, 0.f);

    auto faired = CurveFairing::fair(curve);
    ASSERT_TRUE(faired.isValid());

    float fairedMaxY = maxAbsY(faired);
    EXPECT_LT(fairedMaxY, originalMaxY);
}

TEST(CurveFairing, TooFewPointsReturnsUnchanged)
{
    NurbsCurve shortCurve(
        1,
        {0.f, 0.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f},
            Vec3{1.f, 0.f, 0.f},
        });
    ASSERT_TRUE(shortCurve.isValid());

    auto faired = CurveFairing::fair(shortCurve);
    ASSERT_TRUE(faired.isValid());

    EXPECT_EQ(faired.controlPointCount(), 2);
    EXPECT_NEAR(faired.controlPoints()[0].x, 0.f, 1e-5f);
    EXPECT_NEAR(faired.controlPoints()[1].x, 1.f, 1e-5f);
}

} // namespace
