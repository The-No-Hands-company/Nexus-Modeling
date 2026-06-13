#include <gtest/gtest.h>

#include <nexus/geometry/NurbsCurveFit.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

TEST(NurbsCurveFit, FitsLinearCurveExactly)
{
    std::vector<Vec3> points = {
        {0.f,  0.f, 0.f},
        {5.f,  0.f, 0.f},
        {10.f, 0.f, 0.f},
    };
    NurbsCurveFitOptions opts;
    opts.degree = 1;
    opts.controlPoints = 2;
    auto result = NurbsCurveFit::fit(points, opts);
    EXPECT_LT(result.rmsError, 1e-6f);
    EXPECT_TRUE(result.curve.isValid());
    EXPECT_EQ(result.curve.controlPointCount(), 2);
}

TEST(NurbsCurveFit, FitsCubicLineNearZeroError)
{
    std::vector<Vec3> points = {
        {0.f,  0.f, 0.f},
        {2.f,  0.f, 0.f},
        {5.f,  0.f, 0.f},
        {8.f,  0.f, 0.f},
        {10.f, 0.f, 0.f},
    };
    NurbsCurveFitOptions opts;
    opts.degree = 3;
    opts.controlPoints = 4;
    auto result = NurbsCurveFit::fit(points, opts);
    EXPECT_LT(result.rmsError, 1e-4f);
    EXPECT_TRUE(result.curve.isValid());
}

TEST(NurbsCurveFit, ChordLengthParamProducesValidCurve)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f},
        {3.f, 2.f, 0.f},
        {4.f, 0.f, 0.f},
    };
    NurbsCurveFitOptions opts;
    opts.degree = 2;
    opts.controlPoints = 4;
    opts.uniformParams = false;
    auto result = NurbsCurveFit::fit(points, opts);
    EXPECT_TRUE(result.curve.isValid());
    EXPECT_TRUE(std::isfinite(result.rmsError));
}

TEST(NurbsCurveFit, UniformParamProducesValidCurve)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f},
        {3.f, 2.f, 0.f},
        {4.f, 0.f, 0.f},
    };
    NurbsCurveFitOptions opts;
    opts.degree = 2;
    opts.controlPoints = 4;
    opts.uniformParams = true;
    auto result = NurbsCurveFit::fit(points, opts);
    EXPECT_TRUE(result.curve.isValid());
    EXPECT_TRUE(std::isfinite(result.rmsError));
}

TEST(NurbsCurveFit, TooFewPointsFails)
{
    std::vector<Vec3> empty;
    auto result = NurbsCurveFit::fit(empty);
    EXPECT_FALSE(result.curve.isValid());
    EXPECT_EQ(result.rmsError, 0.f);
    EXPECT_EQ(result.curve.controlPointCount(), 0);
}

TEST(NurbsCurveFit, InvalidDegreeFails)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {2.f, 0.f, 0.f},
    };
    NurbsCurveFitOptions opts;
    opts.degree = 0;
    auto result = NurbsCurveFit::fit(points, opts);
    EXPECT_FALSE(result.curve.isValid());
}

TEST(NurbsCurveFit, TooManyControlPointsFails)
{
    std::vector<Vec3> points = {
        {0.f,  0.f, 0.f},
        {10.f, 0.f, 0.f},
    };
    NurbsCurveFitOptions opts;
    opts.degree = 1;
    opts.controlPoints = 100;
    auto result = NurbsCurveFit::fit(points, opts);
    EXPECT_GT(result.curve.controlPointCount(), static_cast<int32_t>(points.size()))
        << "System is underdetermined: more control points than data points";
    EXPECT_TRUE(std::isfinite(result.rmsError))
        << "Fit should not produce NaN/Inf with excessive CPs";
}

} // namespace
