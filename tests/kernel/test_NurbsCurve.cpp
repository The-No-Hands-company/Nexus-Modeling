#include <nexus/geometry/NurbsCurve.h>
#include <gtest/gtest.h>
#include <cmath>
#include <vector>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static NurbsCurve makeLine()
{
    // Degree-1 clamped line from (0,0,0) to (1,0,0).
    auto c = NurbsCurve::fromControlPoints({{0,0,0},{1,0,0}}, 1);
    return *c;
}

static NurbsCurve makeCubicCurve()
{
    // Degree-3 clamped B-spline with 4 control points.
    auto c = NurbsCurve::fromControlPoints(
        {{0,0,0},{1,1,0},{2,-1,0},{3,0,0}}, 3);
    return *c;
}

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(NurbsCurve, CreateValidReturnsValue)
{
    auto c = NurbsCurve::fromControlPoints({{0,0,0},{1,0,0},{2,0,0}}, 2);
    EXPECT_TRUE(c.has_value());
}

TEST(NurbsCurve, CreateInvalidDegreeZeroReturnsNullopt)
{
    auto c = NurbsCurve::fromControlPoints({{0,0,0},{1,0,0}}, 0);
    EXPECT_FALSE(c.has_value());
}

TEST(NurbsCurve, CreateTooFewControlPointsReturnsNullopt)
{
    // Degree 3 needs at least 4 control points.
    auto c = NurbsCurve::fromControlPoints({{0,0,0},{1,0,0}}, 3);
    EXPECT_FALSE(c.has_value());
}

TEST(NurbsCurve, InconsistentKnotsReturnsNullopt)
{
    NurbsCurveData d;
    d.degree = 2;
    d.controlPoints = {{0,0,0},{1,0,0},{2,0,0}};
    d.weights = {1,1,1};
    d.knots = {0,0,1}; // wrong size (should be 6)
    EXPECT_FALSE(NurbsCurve::create(d).has_value());
}

// ─── Evaluation ───────────────────────────────────────────────────────────────

TEST(NurbsCurve, LinearCurveEvalAtZeroIsStart)
{
    auto c = makeLine();
    auto p = c.evaluate(c.paramMin());
    EXPECT_NEAR(p.x, 0.f, 1e-5f);
    EXPECT_NEAR(p.y, 0.f, 1e-5f);
}

TEST(NurbsCurve, LinearCurveEvalAtOneIsEnd)
{
    auto c = makeLine();
    auto p = c.evaluate(c.paramMax());
    EXPECT_NEAR(p.x, 1.f, 1e-5f);
    EXPECT_NEAR(p.y, 0.f, 1e-5f);
}

TEST(NurbsCurve, LinearCurveEvalAtMidpoint)
{
    auto c = makeLine();
    const double mid = (c.paramMin() + c.paramMax()) * 0.5;
    auto p = c.evaluate(mid);
    EXPECT_NEAR(p.x, 0.5f, 1e-5f);
}

TEST(NurbsCurve, CubicCurveEndsInterpolateControlPoints)
{
    // Clamped B-spline interpolates first and last control points.
    auto c = makeCubicCurve();
    auto p0 = c.evaluate(c.paramMin());
    auto p1 = c.evaluate(c.paramMax());
    EXPECT_NEAR(p0.x, 0.f, 1e-5f);
    EXPECT_NEAR(p0.y, 0.f, 1e-5f);
    EXPECT_NEAR(p1.x, 3.f, 1e-5f);
    EXPECT_NEAR(p1.y, 0.f, 1e-5f);
}

// ─── Derivatives ──────────────────────────────────────────────────────────────

TEST(NurbsCurve, FirstDerivativeNonZeroOnLine)
{
    auto c = makeLine();
    auto d = c.derivative1(0.5);
    // Line from (0,0,0) to (1,0,0): d/du ≈ (1,0,0) scaled by 1/(paramMax-paramMin)
    EXPECT_GT(std::abs(d.x), 0.1f);
}

TEST(NurbsCurve, SecondDerivativeNearZeroOnStraightLine)
{
    auto c = makeLine();
    auto d2 = c.derivative2(0.5);
    // A degree-1 line has zero curvature.
    EXPECT_NEAR(d2.x, 0.f, 0.1f);
    EXPECT_NEAR(d2.y, 0.f, 0.1f);
}

// ─── Tessellation ─────────────────────────────────────────────────────────────

TEST(NurbsCurve, TessellateCountMatchesRequest)
{
    auto c = makeCubicCurve();
    auto pts = c.tessellate(10);
    EXPECT_EQ(pts.size(), 10u);
}

TEST(NurbsCurve, TessellateAdaptiveEndsMatchCurveEndpoints)
{
    auto c = makeCubicCurve();
    auto pts = c.tessellateAdaptive(0.01f);
    ASSERT_GE(pts.size(), 2u);
    // First point ≈ C(paramMin), last ≈ C(paramMax).
    auto p0 = c.evaluate(c.paramMin());
    auto p1 = c.evaluate(c.paramMax());
    EXPECT_NEAR(pts.front().x, p0.x, 0.01f);
    EXPECT_NEAR(pts.back().x,  p1.x, 0.01f);
}

TEST(NurbsCurve, TessellateAdaptiveFinerToleranceMorePoints)
{
    auto c = makeCubicCurve();
    auto coarse = c.tessellateAdaptive(0.5f);
    auto fine   = c.tessellateAdaptive(0.001f);
    EXPECT_GE(fine.size(), coarse.size());
}

// ─── Knot insertion ───────────────────────────────────────────────────────────

TEST(NurbsCurve, KnotInsertionPreservesEndpoints)
{
    auto c = makeCubicCurve();
    const double u = (c.paramMin() + c.paramMax()) * 0.5;
    auto c2 = c.insertKnot(u);
    ASSERT_TRUE(c2.has_value());
    // Curve should be unchanged geometrically at the endpoints.
    auto p0a = c.evaluate(c.paramMin());
    auto p0b = c2->evaluate(c2->paramMin());
    EXPECT_NEAR(p0a.x, p0b.x, 1e-4f);
    EXPECT_NEAR(p0a.y, p0b.y, 1e-4f);
}

TEST(NurbsCurve, KnotInsertionIncreasesControlPointCount)
{
    auto c = makeCubicCurve();
    const double u = (c.paramMin() + c.paramMax()) * 0.5;
    auto c2 = c.insertKnot(u);
    ASSERT_TRUE(c2.has_value());
    EXPECT_EQ(c2->controlPointCount(), c.controlPointCount() + 1);
}

TEST(NurbsCurve, OutOfRangeKnotInsertionReturnsNullopt)
{
    auto c = makeCubicCurve();
    EXPECT_FALSE(c.insertKnot(c.paramMax() + 1.0).has_value());
}

// ─── Degree elevation ─────────────────────────────────────────────────────────

TEST(NurbsCurve, DegreeElevationIncreasesDegreeByOne)
{
    auto c = makeCubicCurve();
    auto c2 = c.elevate();
    EXPECT_EQ(c2.degree(), c.degree() + 1);
}

TEST(NurbsCurve, DegreeElevationPreservesEndpoints)
{
    auto c = makeCubicCurve();
    auto c2 = c.elevate();
    auto p0a = c.evaluate(c.paramMin());
    auto p0b = c2.evaluate(c2.paramMin());
    EXPECT_NEAR(p0a.x, p0b.x, 1e-2f);
    EXPECT_NEAR(p0a.y, p0b.y, 1e-2f);
}
