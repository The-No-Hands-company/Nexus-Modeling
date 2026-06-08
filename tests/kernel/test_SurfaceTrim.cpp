#include <nexus/geometry/SurfaceTrim.h>
#include <nexus/geometry/NurbsSurface.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static NurbsSurface makeUnitPlane()
{
    std::vector<nexus::render::Vec3> pts = {
        {0,0,0},{1,0,0},{0,1,0},{1,1,0},
    };
    return *NurbsSurface::fromGrid(pts, 2, 2, 1, 1);
}

// ─── TrimLoop::contains ───────────────────────────────────────────────────────

TEST(SurfaceTrim, TrimLoopContainsInsidePoint)
{
    TrimLoop loop;
    TrimCurveSegment seg;
    seg.closed = true;
    // CCW unit square in (u,v) space.
    seg.points = {{0,0},{1,0},{1,1},{0,1},{0,0}};
    loop.segments.push_back(std::move(seg));

    EXPECT_TRUE(loop.contains(0.5, 0.5));
}

TEST(SurfaceTrim, TrimLoopDoesNotContainOutsidePoint)
{
    TrimLoop loop;
    TrimCurveSegment seg;
    seg.closed = true;
    seg.points = {{0,0},{1,0},{1,1},{0,1},{0,0}};
    loop.segments.push_back(std::move(seg));

    EXPECT_FALSE(loop.contains(2.0, 0.5));
    EXPECT_FALSE(loop.contains(-0.5, 0.5));
}

TEST(SurfaceTrim, TrimLoopSignedAreaPositiveForCCW)
{
    TrimLoop loop;
    TrimCurveSegment seg;
    seg.closed = true;
    seg.points = {{0,0},{1,0},{1,1},{0,1}};
    loop.segments.push_back(std::move(seg));

    EXPECT_GT(loop.signedArea(), 0.0);
}

// ─── TrimRegion::contains ─────────────────────────────────────────────────────

TEST(SurfaceTrim, TrimRegionContainsInsideOuterLoop)
{
    auto surf = makeUnitPlane();
    auto region = SurfaceTrimmer::fullDomain(surf);
    EXPECT_TRUE(region.contains(0.5, 0.5));
}

TEST(SurfaceTrim, TrimRegionExcludesHole)
{
    auto surf = makeUnitPlane();
    auto region = SurfaceTrimmer::fullDomain(surf);
    // Add a hole in the centre.
    region.holes.push_back(SurfaceTrimmer::circularLoop(0.5, 0.5, 0.2, 32));
    EXPECT_FALSE(region.contains(0.5, 0.5)); // inside hole
    EXPECT_TRUE(region.contains(0.1, 0.1));  // outside hole, inside outer
}

TEST(SurfaceTrim, FullDomainContainsInterior)
{
    auto surf = makeUnitPlane();
    auto region = SurfaceTrimmer::fullDomain(surf);
    EXPECT_TRUE(region.contains(0.25, 0.75));
}

TEST(SurfaceTrim, FullDomainExcludesExterior)
{
    auto surf = makeUnitPlane();
    auto region = SurfaceTrimmer::fullDomain(surf);
    EXPECT_FALSE(region.contains(1.5, 0.5));
}

// ─── circularLoop ─────────────────────────────────────────────────────────────

TEST(SurfaceTrim, CircularLoopHasCorrectSegmentCount)
{
    auto loop = SurfaceTrimmer::circularLoop(0.5, 0.5, 0.3, 16);
    ASSERT_EQ(loop.segments.size(), 1u);
    // 16 + 1 (closing point).
    EXPECT_EQ(loop.segments[0].points.size(), 17u);
}

TEST(SurfaceTrim, CircularLoopContainsCentre)
{
    auto loop = SurfaceTrimmer::circularLoop(0.5, 0.5, 0.3, 32);
    EXPECT_TRUE(loop.contains(0.5, 0.5));
}

TEST(SurfaceTrim, CircularLoopExcludesDistantPoint)
{
    auto loop = SurfaceTrimmer::circularLoop(0.5, 0.5, 0.1, 32);
    EXPECT_FALSE(loop.contains(0.9, 0.9));
}

// ─── SurfaceTrimmer::tessellate ───────────────────────────────────────────────

TEST(SurfaceTrim, TessellateFullDomainHasFaces)
{
    auto surf = makeUnitPlane();
    auto region = SurfaceTrimmer::fullDomain(surf);
    auto mesh = SurfaceTrimmer::tessellate(surf, {region}, {8, 8, true});
    EXPECT_GT(mesh.topology().faceCount(), 0u);
}

TEST(SurfaceTrim, TessellateWithHoleReducesFaces)
{
    auto surf = makeUnitPlane();
    auto full  = SurfaceTrimmer::fullDomain(surf);
    auto holed = full;
    holed.holes.push_back(SurfaceTrimmer::circularLoop(0.5, 0.5, 0.3, 32));

    auto mFull  = SurfaceTrimmer::tessellate(surf, {full},  {16, 16, true});
    auto mHoled = SurfaceTrimmer::tessellate(surf, {holed}, {16, 16, true});
    EXPECT_LT(mHoled.topology().faceCount(), mFull.topology().faceCount());
}

TEST(SurfaceTrim, TessellateEmptyRegionsUsesAllQuads)
{
    auto surf = makeUnitPlane();
    // No regions → all quads kept (inAnyRegion returns true for empty list).
    auto mesh = SurfaceTrimmer::tessellate(surf, {}, {4, 4, true});
    EXPECT_GT(mesh.topology().faceCount(), 0u);
}

TEST(SurfaceTrim, TessellateKeepOutsideInverts)
{
    auto surf = makeUnitPlane();
    auto region = SurfaceTrimmer::fullDomain(surf);
    // keepInside = false → keep everything OUTSIDE the full domain = nothing.
    TrimTessellationOptions opts{8, 8, false};
    auto mesh = SurfaceTrimmer::tessellate(surf, {region}, opts);
    EXPECT_EQ(mesh.topology().faceCount(), 0u);
}
