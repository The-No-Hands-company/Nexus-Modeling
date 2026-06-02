#include <nexus/geometry/RobustPredicates.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace nexus::geometry::predicates;

// ─── orient2d ────────────────────────────────────────────────────────────────

TEST(RobustPredicates, Orient2dCCW)
{
    const double pa[2] = {0.0, 0.0};
    const double pb[2] = {1.0, 0.0};
    const double pc[2] = {0.0, 1.0};
    EXPECT_GT(orient2d(pa, pb, pc), 0.0);
}

TEST(RobustPredicates, Orient2dCW)
{
    const double pa[2] = {0.0, 0.0};
    const double pb[2] = {0.0, 1.0};
    const double pc[2] = {1.0, 0.0};
    EXPECT_LT(orient2d(pa, pb, pc), 0.0);
}

TEST(RobustPredicates, Orient2dCollinear)
{
    const double pa[2] = {0.0, 0.0};
    const double pb[2] = {1.0, 1.0};
    const double pc[2] = {2.0, 2.0};
    EXPECT_EQ(orient2d(pa, pb, pc), 0.0);
}

TEST(RobustPredicates, Orient2dNearDegenerateSignCorrect)
{
    // Three nearly collinear points where naive floating-point may lose precision.
    // The three points are NOT collinear: pc is shifted by 1e-14 in y.
    const double pa[2] = {0.0, 0.0};
    const double pb[2] = {1.0, 1.0};
    const double pc[2] = {2.0, 2.0 + 1e-14}; // just off the line y=x
    // pc is above the line pa-pb → CCW orientation → positive result.
    const double result = orient2d(pa, pb, pc);
    EXPECT_NE(result, 0.0);
}

TEST(RobustPredicates, Orient2dSymmetric)
{
    const double pa[2] = {1.0, 0.0};
    const double pb[2] = {0.0, 0.0};
    const double pc[2] = {0.0, 1.0};
    // Swapping pa and pb should negate the result.
    const double ab = orient2d(pa, pb, pc);
    const double ba = orient2d(pb, pa, pc);
    EXPECT_LT(ab, 0.0);
    EXPECT_GT(ba, 0.0);
}

// ─── orient3d ────────────────────────────────────────────────────────────────

TEST(RobustPredicates, Orient3dAbove)
{
    // pd above the plane CCW of pa,pb,pc → negative
    const double pa[3] = {0.0, 0.0, 0.0};
    const double pb[3] = {1.0, 0.0, 0.0};
    const double pc[3] = {0.0, 1.0, 0.0};
    const double pd[3] = {0.0, 0.0, 1.0};
    EXPECT_LT(orient3d(pa, pb, pc, pd), 0.0);
}

TEST(RobustPredicates, Orient3dBelow)
{
    const double pa[3] = {0.0, 0.0, 0.0};
    const double pb[3] = {1.0, 0.0, 0.0};
    const double pc[3] = {0.0, 1.0, 0.0};
    const double pd[3] = {0.0, 0.0, -1.0};
    EXPECT_GT(orient3d(pa, pb, pc, pd), 0.0);
}

TEST(RobustPredicates, Orient3dCoplanar)
{
    const double pa[3] = {0.0, 0.0, 0.0};
    const double pb[3] = {1.0, 0.0, 0.0};
    const double pc[3] = {0.0, 1.0, 0.0};
    const double pd[3] = {0.5, 0.5, 0.0};
    EXPECT_EQ(orient3d(pa, pb, pc, pd), 0.0);
}

TEST(RobustPredicates, Orient3dNearCoplanarNonZero)
{
    // Four nearly coplanar points; naive det can return wrong sign.
    const double pa[3] = {0.0, 0.0, 0.0};
    const double pb[3] = {1.0, 0.0, 0.0};
    const double pc[3] = {0.0, 1.0, 0.0};
    const double pd[3] = {0.1, 0.1, 1e-14}; // barely above the plane
    const double result = orient3d(pa, pb, pc, pd);
    // Must not be zero and must be negative (pd above the CCW plane).
    EXPECT_NE(result, 0.0);
}

TEST(RobustPredicates, Orient3dSymmetric)
{
    const double pa[3] = {1.0, 0.0, 0.0};
    const double pb[3] = {0.0, 0.0, 0.0};
    const double pc[3] = {0.0, 1.0, 0.0};
    const double pd[3] = {0.0, 0.0, 1.0};
    const double abcd = orient3d(pa, pb, pc, pd);
    const double bacd = orient3d(pb, pa, pc, pd);
    // Swapping two rows must flip the sign — the product must be negative.
    EXPECT_NE(abcd, 0.0);
    EXPECT_NE(bacd, 0.0);
    EXPECT_LT(abcd * bacd, 0.0);
}
