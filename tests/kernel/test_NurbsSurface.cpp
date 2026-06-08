#include <nexus/geometry/NurbsSurface.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

// 2×2 flat grid (bi-linear, degree 1) in the XY plane.
static NurbsSurface makeFlatPlane()
{
    std::vector<nexus::render::Vec3> pts = {
        {0,0,0}, {1,0,0},
        {0,1,0}, {1,1,0},
    };
    return *NurbsSurface::fromGrid(pts, 2, 2, 1, 1);
}

// 4×4 control-point bicubic patch.
static NurbsSurface makeBicubic()
{
    std::vector<nexus::render::Vec3> pts;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            pts.push_back({static_cast<float>(i), static_cast<float>(j), 0.f});
    return *NurbsSurface::fromGrid(pts, 4, 4, 3, 3);
}

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(NurbsSurface, FromGridValidReturnsValue)
{
    auto s = NurbsSurface::fromGrid(
        {{0,0,0},{1,0,0},{0,1,0},{1,1,0}}, 2, 2, 1, 1);
    EXPECT_TRUE(s.has_value());
}

TEST(NurbsSurface, FromGridWrongSizeReturnsNullopt)
{
    std::vector<nexus::render::Vec3> pts(5, {0,0,0});
    auto s = NurbsSurface::fromGrid(pts, 2, 2, 1, 1); // 5 != 4
    EXPECT_FALSE(s.has_value());
}

TEST(NurbsSurface, DegreeZeroReturnsNullopt)
{
    std::vector<nexus::render::Vec3> pts(4, {0,0,0});
    auto s = NurbsSurface::fromGrid(pts, 2, 2, 0, 1);
    EXPECT_FALSE(s.has_value());
}

TEST(NurbsSurface, InsufficientControlPointsReturnsNullopt)
{
    // Degree 3 needs at least 4 points per direction.
    std::vector<nexus::render::Vec3> pts(4, {0,0,0});
    auto s = NurbsSurface::fromGrid(pts, 2, 2, 3, 3);
    EXPECT_FALSE(s.has_value());
}

// ─── Evaluation ───────────────────────────────────────────────────────────────

TEST(NurbsSurface, FlatPlaneCornerAt00)
{
    auto s = makeFlatPlane();
    auto p = s.evaluate(s.paramMinU(), s.paramMinV());
    EXPECT_NEAR(p.x, 0.f, 1e-5f);
    EXPECT_NEAR(p.y, 0.f, 1e-5f);
    EXPECT_NEAR(p.z, 0.f, 1e-5f);
}

TEST(NurbsSurface, FlatPlaneCornerAt11)
{
    auto s = makeFlatPlane();
    auto p = s.evaluate(s.paramMaxU(), s.paramMaxV());
    EXPECT_NEAR(p.x, 1.f, 1e-5f);
    EXPECT_NEAR(p.y, 1.f, 1e-5f);
}

TEST(NurbsSurface, FlatPlaneMidpoint)
{
    auto s = makeFlatPlane();
    const double um = (s.paramMinU() + s.paramMaxU()) * 0.5;
    const double vm = (s.paramMinV() + s.paramMaxV()) * 0.5;
    auto p = s.evaluate(um, vm);
    EXPECT_NEAR(p.x, 0.5f, 1e-4f);
    EXPECT_NEAR(p.y, 0.5f, 1e-4f);
}

TEST(NurbsSurface, BicubicCornersInterpolateControlPoints)
{
    auto s = makeBicubic();
    auto p00 = s.evaluate(s.paramMinU(), s.paramMinV());
    auto p11 = s.evaluate(s.paramMaxU(), s.paramMaxV());
    EXPECT_NEAR(p00.x, 0.f, 1e-4f);
    EXPECT_NEAR(p00.y, 0.f, 1e-4f);
    EXPECT_NEAR(p11.x, 3.f, 1e-4f);
    EXPECT_NEAR(p11.y, 3.f, 1e-4f);
}

// ─── Derivatives ──────────────────────────────────────────────────────────────

TEST(NurbsSurface, DerivUNonZeroOnFlatPlane)
{
    auto s = makeFlatPlane();
    auto d = s.derivU(0.5, 0.5);
    // flat plane: S(u,v) maps u to one axis and v to the other — at least one
    // partial is non-zero along each direction.
    EXPECT_GT(std::abs(d.x) + std::abs(d.y), 0.1f);
}

TEST(NurbsSurface, DerivVNonZeroOnFlatPlane)
{
    auto s = makeFlatPlane();
    auto d = s.derivV(0.5, 0.5);
    EXPECT_GT(std::abs(d.x) + std::abs(d.y), 0.1f);
}

TEST(NurbsSurface, NormalOnFlatXYPlanePointsInZ)
{
    auto s = makeFlatPlane();
    auto n = s.normal(0.5, 0.5);
    EXPECT_NEAR(std::abs(n.z), 1.f, 1e-4f);
}

// ─── Tessellation ─────────────────────────────────────────────────────────────

TEST(NurbsSurface, TessellateVertexCount)
{
    auto s = makeFlatPlane();
    auto m = s.tessellate(4, 4);
    EXPECT_EQ(m.attributes().positions().size(), 4u * 4u);
}

TEST(NurbsSurface, TessellateFaceCount)
{
    auto s = makeFlatPlane();
    auto m = s.tessellate(4, 4); // 3×3 quads × 2 = 18 triangles
    EXPECT_EQ(m.topology().faceCount(), 3u * 3u * 2u);
}

TEST(NurbsSurface, TessellateAdaptiveNoCrash)
{
    auto s = makeBicubic();
    auto m = s.tessellateAdaptive(0.1f);
    EXPECT_GT(m.topology().faceCount(), 0u);
}

// ─── Iso-parameter curves ─────────────────────────────────────────────────────

TEST(NurbsSurface, IsoUCurveEvalMatchesSurface)
{
    auto s = makeFlatPlane();
    const double uConst = (s.paramMinU() + s.paramMaxU()) * 0.5;
    auto curve = s.isoU(uConst);
    ASSERT_TRUE(curve.has_value());
    // At v = paramMin, the curve should match S(u=uConst, v=paramMin).
    auto pCurve   = curve->evaluate(curve->paramMin());
    auto pSurface = s.evaluate(uConst, s.paramMinV());
    EXPECT_NEAR(pCurve.x, pSurface.x, 1e-3f);
    EXPECT_NEAR(pCurve.y, pSurface.y, 1e-3f);
}

TEST(NurbsSurface, IsoVCurveEvalMatchesSurface)
{
    auto s = makeFlatPlane();
    const double vConst = (s.paramMinV() + s.paramMaxV()) * 0.5;
    auto curve = s.isoV(vConst);
    ASSERT_TRUE(curve.has_value());
    auto pCurve   = curve->evaluate(curve->paramMin());
    auto pSurface = s.evaluate(s.paramMinU(), vConst);
    EXPECT_NEAR(pCurve.y, pSurface.y, 1e-3f);
}

// ─── Knot insertion ───────────────────────────────────────────────────────────

TEST(NurbsSurface, KnotInsertUIncreasesNu)
{
    auto s = makeBicubic();
    const double u = (s.paramMinU() + s.paramMaxU()) * 0.5;
    auto s2 = s.insertKnotU(u);
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s2->data().nU, s.data().nU + 1);
}

TEST(NurbsSurface, KnotInsertVIncreasesNv)
{
    auto s = makeBicubic();
    const double v = (s.paramMinV() + s.paramMaxV()) * 0.5;
    auto s2 = s.insertKnotV(v);
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s2->data().nV, s.data().nV + 1);
}

TEST(NurbsSurface, KnotInsertOutOfRangeReturnsNullopt)
{
    auto s = makeBicubic();
    EXPECT_FALSE(s.insertKnotU(s.paramMaxU() + 1.0).has_value());
    EXPECT_FALSE(s.insertKnotV(s.paramMinV() - 1.0).has_value());
}

TEST(NurbsSurface, KnotInsertPreservesShape)
{
    auto s = makeBicubic();
    const double u = (s.paramMinU() + s.paramMaxU()) * 0.5;
    auto s2 = s.insertKnotU(u);
    ASSERT_TRUE(s2.has_value());
    // Both surfaces should evaluate to the same point at a test location.
    auto p1 = s.evaluate(0.3, 0.7);
    auto p2 = s2->evaluate(0.3, 0.7);
    EXPECT_NEAR(p1.x, p2.x, 1e-3f);
    EXPECT_NEAR(p1.y, p2.y, 1e-3f);
}
