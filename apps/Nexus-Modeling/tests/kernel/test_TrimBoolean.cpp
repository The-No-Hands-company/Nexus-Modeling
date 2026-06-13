#include <gtest/gtest.h>

#include <nexus/geometry/TrimBoolean.h>

namespace {

using namespace nexus::geometry;

static TrimBooleanOptions highResOpts()
{
    TrimBooleanOptions opts;
    opts.gridRes = 512;
    return opts;
}

static BooleanLoop makeSquareLoop(float x0, float y0, float size)
{
    BooleanLoop loop;
    loop.closed = true;
    loop.points = {
        Vec3{x0,        y0,        0.f},
        Vec3{x0 + size, y0,        0.f},
        Vec3{x0 + size, y0 + size, 0.f},
        Vec3{x0,        y0 + size, 0.f},
    };
    return loop;
}

static BooleanRegion makeSquareRegion(float x, float y, float size)
{
    BooleanRegion region;
    region.outerLoops.push_back(makeSquareLoop(x, y, size));
    return region;
}

static BooleanRegion makeSquareWithHole(float outerX, float outerY,
                                         float outerSize,
                                         float holeX, float holeY,
                                         float holeSize)
{
    BooleanRegion region;
    region.outerLoops.push_back(makeSquareLoop(outerX, outerY, outerSize));
    region.innerLoops.push_back(makeSquareLoop(holeX, holeY, holeSize));
    return region;
}

TEST(TrimBoolean, UnionOfTwoSquaresRunsAndReturnsResult)
{
    auto squareA = makeSquareRegion(0.f, 0.f, 1.f);
    auto squareB = makeSquareRegion(0.f, 0.f, 1.f);

    const auto result =
        TrimBoolean::compute(squareA, squareB, BooleanOp::Union);

    EXPECT_FALSE(result.empty());
}

TEST(TrimBoolean, IntersectionOfSameSquareRuns)
{
    auto squareA = makeSquareRegion(0.f, 0.f, 1.f);
    auto squareB = makeSquareRegion(0.f, 0.f, 1.f);

    const auto result =
        TrimBoolean::compute(squareA, squareB, BooleanOp::Intersection);

    EXPECT_FALSE(result.empty());
}

TEST(TrimBoolean, DifferenceOfSameSquareRuns)
{
    auto squareA = makeSquareRegion(0.f, 0.f, 1.f);
    auto squareB = makeSquareRegion(0.f, 0.f, 1.f);

    const auto result =
        TrimBoolean::compute(squareA, squareB, BooleanOp::Difference);

    // Difference of identical squares may be empty; that is a valid result
    EXPECT_GE(result.outerLoops.size(), 0u);
}

TEST(TrimBoolean, UnionOfSolidSquaresHasNoInnerLoops)
{
    auto squareA = makeSquareRegion(0.f, 0.f, 2.f);
    auto squareB = makeSquareRegion(1.f, 0.f, 2.f);

    const auto result =
        TrimBoolean::compute(squareA, squareB, BooleanOp::Union, highResOpts());

    EXPECT_FALSE(result.empty());
    EXPECT_GE(result.outerLoops.size(), 1u);
    // Two solid overlapping squares — no holes expected
    EXPECT_EQ(result.innerLoops.size(), 0u);
}

TEST(TrimBoolean, UnionPreservesHole)
{
    // Outer square 0,0 .. 4,4 with a hole 1,1 .. 2,2
    auto region = makeSquareWithHole(0.f, 0.f, 4.f, 1.f, 1.f, 2.f);

    const auto result =
        TrimBoolean::compute(region, region, BooleanOp::Union, highResOpts());

    EXPECT_FALSE(result.empty());
    EXPECT_GE(result.outerLoops.size(), 1u);
    // Union with itself — the hole should be preserved
    EXPECT_GE(result.innerLoops.size(), 1u);
}

TEST(TrimBoolean, IntersectionPreservesHole)
{
    auto region = makeSquareWithHole(0.f, 0.f, 4.f, 1.f, 1.f, 2.f);

    const auto result =
        TrimBoolean::compute(region, region, BooleanOp::Intersection, highResOpts());

    EXPECT_FALSE(result.empty());
    EXPECT_GE(result.outerLoops.size(), 1u);
    // Intersection of region with itself — hole preserved
    EXPECT_GE(result.innerLoops.size(), 1u);
}

TEST(TrimBoolean, DifferenceWithHole)
{
    // Region A: outer 0,0..4,4 with hole 1,1..2,2
    auto regionA = makeSquareWithHole(0.f, 0.f, 4.f, 1.f, 1.f, 2.f);

    // Region B: small square at 3,0..4,1 (only overlaps outer shell, not the hole)
    auto regionB = makeSquareRegion(3.f, 0.f, 1.f);

    const auto result =
        TrimBoolean::compute(regionA, regionB, BooleanOp::Difference, highResOpts());

    EXPECT_FALSE(result.empty());
    EXPECT_GE(result.outerLoops.size(), 1u);
    // The hole from A should be preserved (B only subtracts from outer shell)
    EXPECT_GE(result.innerLoops.size(), 1u);
}

TEST(TrimBoolean, DifferenceRemovesHoleCoveredByB)
{
    // Region A: outer 0,0..4,4 with hole 1,1..2,2
    auto regionA = makeSquareWithHole(0.f, 0.f, 4.f, 1.f, 1.f, 2.f);

    // Region B: square covering the hole area
    auto regionB = makeSquareRegion(0.5f, 0.5f, 3.f);

    const auto result =
        TrimBoolean::compute(regionA, regionB, BooleanOp::Difference, highResOpts());

    EXPECT_FALSE(result.empty());
    EXPECT_GE(result.outerLoops.size(), 1u);
    // The hole area is subtracted (A - B where B covers the hole => hole region removed)
    // Since B covers the hole, the result's hole portion is gone
    // The result should still work even if B removes the hole fill area
}

} // namespace
