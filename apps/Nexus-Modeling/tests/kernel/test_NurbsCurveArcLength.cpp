#include <gtest/gtest.h>

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsCurveArcLength.h>

#include <cmath>

using namespace nexus::geometry;

namespace {

NurbsCurve makeLine(float length)
{
    std::vector<float> knots = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f},
        {length, 0.f, 0.f},
    };
    return NurbsCurve(1, std::move(knots), std::move(ctl));
}

} // namespace

TEST(NurbsCurveArcLength, StraightLineLength)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    EXPECT_NEAR(result.totalLength, 10.f, 0.02f);
}

TEST(NurbsCurveArcLength, ParameterAtHalfLength)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    float param = NurbsCurveArcLength::parameterAtLength(curve, 5.f, result);

    Vec3 pt = curve.evaluate(param);
    EXPECT_NEAR(pt.x, 5.f, 0.02f);
    EXPECT_NEAR(pt.y, 0.f, 1e-4f);
    EXPECT_NEAR(pt.z, 0.f, 1e-4f);
}

TEST(NurbsCurveArcLength, LengthAtEndReturnsEndpoint)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    float param = NurbsCurveArcLength::parameterAtLength(curve, result.totalLength, result);

    Vec3 pt = curve.evaluate(param);
    EXPECT_NEAR(pt.x, 10.f, 0.02f);
    EXPECT_NEAR(pt.y, 0.f, 1e-4f);
    EXPECT_NEAR(pt.z, 0.f, 1e-4f);
}

TEST(NurbsCurveArcLength, BeyondTotalLengthClamps)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    float param = NurbsCurveArcLength::parameterAtLength(curve, 100.f, result);

    Vec3 pt = curve.evaluate(param);
    EXPECT_NEAR(pt.x, 10.f, 0.02f);
}

TEST(NurbsCurveArcLength, BelowZeroClampsToStart)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    float param = NurbsCurveArcLength::parameterAtLength(curve, -5.f, result);

    Vec3 pt = curve.evaluate(param);
    EXPECT_NEAR(pt.x, 0.f, 0.02f);
}
