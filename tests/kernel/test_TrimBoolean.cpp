#include <nexus/geometry/TrimBoolean.h>
#include <nexus/geometry/SurfaceTrim.h>
#include <nexus/geometry/NurbsSurface.h>
#include <gtest/gtest.h>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static NurbsSurface makeUnitPlane()
{
    std::vector<nexus::render::Vec3> pts = {
        {0,0,0},{1,0,0},{0,1,0},{1,1,0},
    };
    return *NurbsSurface::fromGrid(pts, 2, 2, 1, 1);
}

// Left half: u ∈ [0, 0.6]
static TrimRegion makeLeftHalf()
{
    TrimRegion r;
    TrimCurveSegment seg;
    seg.closed = true;
    seg.points = {{0,0},{0.6,0},{0.6,1},{0,1},{0,0}};
    r.outer.segments.push_back(std::move(seg));
    return r;
}

// Right half: u ∈ [0.4, 1]  — overlaps left half in [0.4, 0.6]
static TrimRegion makeRightHalf()
{
    TrimRegion r;
    TrimCurveSegment seg;
    seg.closed = true;
    seg.points = {{0.4,0},{1,0},{1,1},{0.4,1},{0.4,0}};
    r.outer.segments.push_back(std::move(seg));
    return r;
}

// ─── TrimBooleanOp::contains ──────────────────────────────────────────────────

TEST(TrimBoolean, UnionContainsBothHalves)
{
    auto a = makeLeftHalf();
    auto b = makeRightHalf();
    TrimBooleanOp op(a, b, TrimBooleanType::Union);
    EXPECT_TRUE(op.contains(0.1, 0.5));   // left only
    EXPECT_TRUE(op.contains(0.9, 0.5));   // right only
    EXPECT_TRUE(op.contains(0.5, 0.5));   // overlap
}

TEST(TrimBoolean, UnionExcludesExterior)
{
    auto a = makeLeftHalf();
    auto b = makeRightHalf();
    TrimBooleanOp op(a, b, TrimBooleanType::Union);
    // Neither region covers (1.5, 0.5).
    EXPECT_FALSE(op.contains(1.5, 0.5));
    EXPECT_FALSE(op.contains(-0.1, 0.5));
}

TEST(TrimBoolean, IntersectionContainsOverlapOnly)
{
    auto a = makeLeftHalf();
    auto b = makeRightHalf();
    TrimBooleanOp op(a, b, TrimBooleanType::Intersection);
    EXPECT_TRUE(op.contains(0.5, 0.5));   // in both
    EXPECT_FALSE(op.contains(0.1, 0.5));  // left only, not in right
    EXPECT_FALSE(op.contains(0.9, 0.5));  // right only, not in left
}

TEST(TrimBoolean, DifferenceExcludesB)
{
    auto a = makeLeftHalf();
    auto b = makeRightHalf();
    TrimBooleanOp op(a, b, TrimBooleanType::Difference);
    EXPECT_TRUE(op.contains(0.1, 0.5));   // in A, not in B
    EXPECT_FALSE(op.contains(0.5, 0.5));  // in both → excluded
    EXPECT_FALSE(op.contains(0.9, 0.5));  // in B only → excluded
}

// ─── toExplicitRegion ─────────────────────────────────────────────────────────

TEST(TrimBoolean, UnionExplicitContainsInterior)
{
    auto a = makeLeftHalf();
    auto b = makeRightHalf();
    auto region = trimUnion(a, b, 32, 32);
    EXPECT_TRUE(region.contains(0.1, 0.5));
    EXPECT_TRUE(region.contains(0.9, 0.5));
    EXPECT_TRUE(region.contains(0.5, 0.5));
}

TEST(TrimBoolean, IntersectionExplicitContainsOverlap)
{
    auto a = makeLeftHalf();
    auto b = makeRightHalf();
    auto region = trimIntersection(a, b, 32, 32);
    // Use v=0.3 to avoid landing on cell boundaries (multiples of 1/32).
    EXPECT_TRUE(region.contains(0.5, 0.3));   // in both A and B
    EXPECT_FALSE(region.contains(0.2, 0.3));  // in A only
    EXPECT_FALSE(region.contains(0.8, 0.3));  // in B only
}

TEST(TrimBoolean, DifferenceExplicitCorrect)
{
    auto a = makeLeftHalf();
    auto b = makeRightHalf();
    auto region = trimDifference(a, b, 32, 32);
    // A - B keeps u ∈ [0, 0.4); use v=0.3 to avoid cell boundaries (k/32).
    EXPECT_TRUE(region.contains(0.2,  0.3));  // in A, not in B
    EXPECT_FALSE(region.contains(0.51, 0.3)); // in both → excluded (0.51 ≠ k/32)
    EXPECT_FALSE(region.contains(0.8,  0.3)); // in B only → excluded
}

// ─── Integration: tessellate trimmed surface with Boolean region ───────────────

TEST(TrimBoolean, UnionTessellateHasFaces)
{
    auto surf = makeUnitPlane();
    auto a = SurfaceTrimmer::fullDomain(surf);
    // b is a circle covering centre region.
    auto bLoop = SurfaceTrimmer::circularLoop(0.5, 0.5, 0.4, 16);
    TrimRegion b;
    b.outer = bLoop;
    auto region = trimUnion(a, b, 16, 16);
    TrimTessellationOptions opts{16, 16, true};
    auto mesh = SurfaceTrimmer::tessellate(surf, {region}, opts);
    EXPECT_GT(mesh.topology().faceCount(), 0u);
}

TEST(TrimBoolean, IntersectionTessellateFewerFacesThanFull)
{
    auto surf = makeUnitPlane();
    auto full  = SurfaceTrimmer::fullDomain(surf);
    // Intersection of full domain with left half = left half.
    auto left  = makeLeftHalf();
    auto region = trimIntersection(full, left, 32, 32);
    TrimTessellationOptions opts{16, 16, true};
    auto mFull   = SurfaceTrimmer::tessellate(surf, {full},   opts);
    auto mInter  = SurfaceTrimmer::tessellate(surf, {region}, opts);
    EXPECT_LT(mInter.topology().faceCount(), mFull.topology().faceCount());
}

TEST(TrimBoolean, DifferenceTessellateFewerFacesThanA)
{
    auto surf = makeUnitPlane();
    auto full  = SurfaceTrimmer::fullDomain(surf);
    auto right = makeRightHalf();
    auto region = trimDifference(full, right, 32, 32);
    TrimTessellationOptions opts{16, 16, true};
    auto mFull = SurfaceTrimmer::tessellate(surf, {full},   opts);
    auto mDiff = SurfaceTrimmer::tessellate(surf, {region}, opts);
    EXPECT_LT(mDiff.topology().faceCount(), mFull.topology().faceCount());
}
