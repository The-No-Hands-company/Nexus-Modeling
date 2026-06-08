#include <nexus/geometry/SweepLoft.h>
#include <nexus/geometry/NurbsCurve.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static NurbsCurve makeLinearSpine()
{
    // Straight line along Z from (0,0,0) to (0,0,3).
    std::vector<nexus::render::Vec3> pts = {{0,0,0},{0,0,1},{0,0,2},{0,0,3}};
    return *NurbsCurve::fromControlPoints(pts, 1);
}

static NurbsCurve makeCubicSpine()
{
    std::vector<nexus::render::Vec3> pts = {{0,0,0},{1,0,1},{0,0,2},{0,0,3}};
    return *NurbsCurve::fromControlPoints(pts, 3);
}

static std::vector<nexus::render::Vec3> makeSquareProfile()
{
    return {{0.5f,0.5f,0},{-0.5f,0.5f,0},{-0.5f,-0.5f,0},{0.5f,-0.5f,0}};
}

// ─── Sweeper::sweep ───────────────────────────────────────────────────────────

TEST(SweepLoft, SweepReturnsOk)
{
    auto spine = makeLinearSpine();
    auto result = Sweeper::sweep(spine, makeSquareProfile(), {});
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.error.empty());
}

TEST(SweepLoft, SweepHasFaces)
{
    auto spine = makeLinearSpine();
    SweepOptions opts;
    opts.spineSteps = 8;
    auto result = Sweeper::sweep(spine, makeSquareProfile(), opts);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}

TEST(SweepLoft, SweepHasVertices)
{
    auto spine = makeLinearSpine();
    SweepOptions opts;
    opts.spineSteps = 4;
    auto result = Sweeper::sweep(spine, makeSquareProfile(), opts);
    // 4 steps × 4 profile points = 16 vertices minimum.
    EXPECT_GE(result.mesh.attributes().positions().size(), 16u);
}

TEST(SweepLoft, SweepEmptyProfileFails)
{
    auto spine = makeLinearSpine();
    auto result = Sweeper::sweep(spine, {}, {});
    EXPECT_FALSE(result.ok);
}

TEST(SweepLoft, SweepSinglePointProfileFails)
{
    auto spine = makeLinearSpine();
    auto result = Sweeper::sweep(spine, {{0,0,0}}, {});
    EXPECT_FALSE(result.ok);
}

TEST(SweepLoft, SweepWithCapStartAddsExtraVertex)
{
    auto spine = makeLinearSpine();
    SweepOptions opts;
    opts.spineSteps   = 4;
    opts.capStart     = true;
    opts.capEnd       = false;
    SweepOptions noCapOpts2;
    noCapOpts2.spineSteps = 4;
    noCapOpts2.capStart   = false;
    noCapOpts2.capEnd     = false;
    auto noCap = Sweeper::sweep(spine, makeSquareProfile(), noCapOpts2);
    auto withCap = Sweeper::sweep(spine, makeSquareProfile(), opts);
    EXPECT_GT(withCap.mesh.attributes().positions().size(),
              noCap.mesh.attributes().positions().size());
}

TEST(SweepLoft, SweepCapStartAddsFaces)
{
    auto spine = makeLinearSpine();
    SweepOptions opts;
    opts.spineSteps = 4;
    opts.capStart   = true;
    opts.capEnd     = true;
    auto result = Sweeper::sweep(spine, makeSquareProfile(), opts);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}

TEST(SweepLoft, SweepCubicSpineOk)
{
    auto spine = makeCubicSpine();
    SweepOptions opts;
    opts.spineSteps = 16;
    auto result = Sweeper::sweep(spine, makeSquareProfile(), opts);
    EXPECT_TRUE(result.ok);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}

TEST(SweepLoft, SweepWithTwistHasSameFaceCount)
{
    auto spine = makeLinearSpine();
    SweepOptions optsNoTwist;
    optsNoTwist.spineSteps = 8;
    SweepOptions optsTwist = optsNoTwist;
    optsTwist.twistRadians = static_cast<float>(M_PI);

    auto r0 = Sweeper::sweep(spine, makeSquareProfile(), optsNoTwist);
    auto r1 = Sweeper::sweep(spine, makeSquareProfile(), optsTwist);
    EXPECT_EQ(r0.mesh.topology().faceCount(), r1.mesh.topology().faceCount());
}

// ─── Sweeper::sweepCircle ─────────────────────────────────────────────────────

TEST(SweepLoft, SweepCircleReturnsOk)
{
    auto spine = makeLinearSpine();
    auto result = Sweeper::sweepCircle(spine, 1.0f, {});
    EXPECT_TRUE(result.ok);
}

TEST(SweepLoft, SweepCircleHasFaces)
{
    auto spine = makeLinearSpine();
    SweepOptions opts;
    opts.spineSteps   = 8;
    opts.profileSteps = 16;
    auto result = Sweeper::sweepCircle(spine, 0.5f, opts);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}

// ─── Lofter::loft ─────────────────────────────────────────────────────────────

TEST(SweepLoft, LoftTwoProfilesReturnsOk)
{
    std::vector<nexus::render::Vec3> p0 = {{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
    std::vector<nexus::render::Vec3> p1 = {{0,0,2},{1,0,2},{1,1,2},{0,1,2}};
    auto result = Lofter::loft({p0, p1}, {});
    EXPECT_TRUE(result.ok);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}

TEST(SweepLoft, LoftOneProfileFails)
{
    std::vector<nexus::render::Vec3> p0 = {{0,0,0},{1,0,0}};
    auto result = Lofter::loft({p0}, {});
    EXPECT_FALSE(result.ok);
}

TEST(SweepLoft, LoftMismatchedProfilesFails)
{
    std::vector<nexus::render::Vec3> p0 = {{0,0,0},{1,0,0},{1,1,0}};
    std::vector<nexus::render::Vec3> p1 = {{0,0,1},{1,0,1}};
    auto result = Lofter::loft({p0, p1}, {});
    EXPECT_FALSE(result.ok);
}

TEST(SweepLoft, LoftThreeProfilesHasMoreFacesThanTwo)
{
    std::vector<nexus::render::Vec3> p0 = {{0,0,0},{1,0,0},{0,1,0}};
    std::vector<nexus::render::Vec3> p1 = {{0,0,1},{1,0,1},{0,1,1}};
    std::vector<nexus::render::Vec3> p2 = {{0,0,2},{1,0,2},{0,1,2}};
    auto r2 = Lofter::loft({p0, p1}, {});
    auto r3 = Lofter::loft({p0, p1, p2}, {});
    EXPECT_GT(r3.mesh.topology().faceCount(), r2.mesh.topology().faceCount());
}

TEST(SweepLoft, LoftWithCapsAddsFaces)
{
    std::vector<nexus::render::Vec3> p0 = {{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
    std::vector<nexus::render::Vec3> p1 = {{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    LoftOptions opts;
    opts.capStart = true;
    opts.capEnd   = true;
    auto withCaps = Lofter::loft({p0, p1}, opts);
    LoftOptions noCapOpts;
    noCapOpts.capStart = false;
    noCapOpts.capEnd   = false;
    auto noCaps   = Lofter::loft({p0, p1}, noCapOpts);
    EXPECT_GT(withCaps.mesh.topology().faceCount(), noCaps.mesh.topology().faceCount());
}

// ─── Lofter::loftCurves ───────────────────────────────────────────────────────

TEST(SweepLoft, LoftCurvesTwoCurvesOk)
{
    std::vector<nexus::render::Vec3> c0pts = {{0,0,0},{1,0,0},{2,0,0},{3,0,0}};
    std::vector<nexus::render::Vec3> c1pts = {{0,1,1},{1,1,1},{2,1,1},{3,1,1}};
    auto c0 = *NurbsCurve::fromControlPoints(c0pts, 3);
    auto c1 = *NurbsCurve::fromControlPoints(c1pts, 3);
    std::vector<NurbsCurve> curves{c0, c1};
    auto result = Lofter::loftCurves(curves, 8, {});
    EXPECT_TRUE(result.ok);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}
