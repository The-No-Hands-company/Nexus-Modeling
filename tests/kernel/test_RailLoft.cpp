#include <nexus/geometry/RailLoft.h>
#include <nexus/geometry/NurbsCurve.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static NurbsCurve makeLinearRail(float x0, float x1, float y, float z0, float z1)
{
    std::vector<nexus::render::Vec3> pts = {
        {x0, y, z0}, {x0, y, (z0+z1)*0.5f}, {x1, y, (z0+z1)*0.5f}, {x1, y, z1}
    };
    return *NurbsCurve::fromControlPoints(pts, 3);
}

static std::vector<nexus::render::Vec3> makeFlatProfile(uint32_t n = 4)
{
    // Normalized span profile: x from 0 to 1, y = 0 (flat).
    std::vector<nexus::render::Vec3> p;
    for (uint32_t i = 0; i < n; ++i) {
        p.push_back({static_cast<float>(i) / (n - 1), 0.f, 0.f});
    }
    return p;
}

// ─── RailLofter::loft ─────────────────────────────────────────────────────────

TEST(RailLoft, TwoParallelRailsReturnsOk)
{
    auto rail0 = makeLinearRail(0, 0, 0, 0, 3);
    auto rail1 = makeLinearRail(1, 1, 0, 0, 3);
    auto result = RailLofter::loft(rail0, rail1, makeFlatProfile(), {});
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.error.empty());
}

TEST(RailLoft, TwoParallelRailsHasFaces)
{
    auto rail0 = makeLinearRail(0, 0, 0, 0, 3);
    auto rail1 = makeLinearRail(1, 1, 0, 0, 3);
    RailLoftOptions opts;
    opts.railSteps = 8;
    auto result = RailLofter::loft(rail0, rail1, makeFlatProfile(), opts);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}

TEST(RailLoft, TwoParallelRailsHasVertices)
{
    auto rail0 = makeLinearRail(0, 0, 0, 0, 3);
    auto rail1 = makeLinearRail(1, 1, 0, 0, 3);
    RailLoftOptions opts;
    opts.railSteps    = 4;
    opts.capStart     = false;
    opts.capEnd       = false;
    auto result = RailLofter::loft(rail0, rail1, makeFlatProfile(4), opts);
    // 4 steps × 4 profile points = 16 vertices.
    EXPECT_EQ(result.mesh.attributes().positions().size(), 16u);
}

TEST(RailLoft, EmptyProfileFails)
{
    auto rail0 = makeLinearRail(0, 0, 0, 0, 1);
    auto rail1 = makeLinearRail(1, 1, 0, 0, 1);
    auto result = RailLofter::loft(rail0, rail1, {}, {});
    EXPECT_FALSE(result.ok);
}

TEST(RailLoft, SinglePointProfileFails)
{
    auto rail0 = makeLinearRail(0, 0, 0, 0, 1);
    auto rail1 = makeLinearRail(1, 1, 0, 0, 1);
    auto result = RailLofter::loft(rail0, rail1, {{0.5f, 0.f, 0.f}}, {});
    EXPECT_FALSE(result.ok);
}

TEST(RailLoft, CapStartAndEndAddVertices)
{
    auto rail0 = makeLinearRail(0, 0, 0, 0, 3);
    auto rail1 = makeLinearRail(1, 1, 0, 0, 3);
    RailLoftOptions optsCap;
    optsCap.railSteps = 4;
    optsCap.capStart  = true;
    optsCap.capEnd    = true;
    RailLoftOptions optsNoCap;
    optsNoCap.railSteps = 4;
    optsNoCap.capStart  = false;
    optsNoCap.capEnd    = false;
    auto withCap = RailLofter::loft(rail0, rail1, makeFlatProfile(4), optsCap);
    auto noCap   = RailLofter::loft(rail0, rail1, makeFlatProfile(4), optsNoCap);
    EXPECT_GT(withCap.mesh.attributes().positions().size(),
              noCap.mesh.attributes().positions().size());
}

TEST(RailLoft, MoreRailStepsGivesMoreFaces)
{
    auto rail0 = makeLinearRail(0, 0, 0, 0, 3);
    auto rail1 = makeLinearRail(1, 1, 0, 0, 3);
    RailLoftOptions opts4, opts8;
    opts4.railSteps = 4; opts4.capStart = false; opts4.capEnd = false;
    opts8.railSteps = 8; opts8.capStart = false; opts8.capEnd = false;
    auto r4 = RailLofter::loft(rail0, rail1, makeFlatProfile(4), opts4);
    auto r8 = RailLofter::loft(rail0, rail1, makeFlatProfile(4), opts8);
    EXPECT_GT(r8.mesh.topology().faceCount(), r4.mesh.topology().faceCount());
}

TEST(RailLoft, DivergingRailsProducesValidMesh)
{
    // One rail straight, one rail angled (diverging span).
    auto rail0 = makeLinearRail(0, 0, 0, 0, 4);
    auto rail1 = makeLinearRail(1, 2, 0, 0, 4);   // x goes from 1 to 2
    RailLoftOptions opts;
    opts.railSteps = 8;
    auto result = RailLofter::loft(rail0, rail1, makeFlatProfile(), opts);
    EXPECT_TRUE(result.ok);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}

// ─── RailLofter::loftFlat ────────────────────────────────────────────────────

TEST(RailLoft, LoftFlatReturnsOk)
{
    auto rail0 = makeLinearRail(0, 0, 0, 0, 3);
    auto rail1 = makeLinearRail(1, 1, 0, 0, 3);
    auto result = RailLofter::loftFlat(rail0, rail1, 4, {});
    EXPECT_TRUE(result.ok);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}

TEST(RailLoft, LoftFlatSingleStepClampedTo2)
{
    auto rail0 = makeLinearRail(0, 0, 0, 0, 3);
    auto rail1 = makeLinearRail(1, 1, 0, 0, 3);
    // profileSteps=1 should be clamped to 2.
    auto result = RailLofter::loftFlat(rail0, rail1, 1, {});
    EXPECT_TRUE(result.ok);
    EXPECT_GT(result.mesh.topology().faceCount(), 0u);
}
