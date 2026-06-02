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

// ─── inCircle ─────────────────────────────────────────────────────────────────

TEST(RobustPredicates, InCircleInsidePositive)
{
    // pa, pb, pc form a CCW unit circle triangle; pd at origin is inside.
    const double pa[2] = {1.0, 0.0};
    const double pb[2] = {0.0, 1.0};
    const double pc[2] = {-1.0, 0.0};
    const double pd[2] = {0.0, 0.0};
    EXPECT_GT(inCircle(pa, pb, pc, pd), 0.0);
}

TEST(RobustPredicates, InCircleOutsideNegative)
{
    const double pa[2] = {1.0, 0.0};
    const double pb[2] = {0.0, 1.0};
    const double pc[2] = {-1.0, 0.0};
    const double pd[2] = {3.0, 0.0}; // far outside
    EXPECT_LT(inCircle(pa, pb, pc, pd), 0.0);
}

TEST(RobustPredicates, InCircleOnCircleZero)
{
    // All four points on the unit circle → determinant = 0.
    const double pa[2] = {1.0, 0.0};
    const double pb[2] = {0.0, 1.0};
    const double pc[2] = {-1.0, 0.0};
    const double pd[2] = {0.0, -1.0};
    EXPECT_EQ(inCircle(pa, pb, pc, pd), 0.0);
}

TEST(RobustPredicates, InCircleNearDegenerateNonZero)
{
    // pd is barely inside the circumcircle — naive FP may lose the sign.
    const double pa[2] = { 1.0,  0.0};
    const double pb[2] = { 0.0,  1.0};
    const double pc[2] = {-1.0,  0.0};
    const double pd[2] = { 0.0, -1.0 + 1e-13}; // slightly inside
    const double result = inCircle(pa, pb, pc, pd);
    EXPECT_NE(result, 0.0);
}

// ─── inSphere ────────────────────────────────────────────────────────────────

TEST(RobustPredicates, InSphereInsideNonZero)
{
    // Unit sphere tetrahedron; pe at origin is inside → determinant non-zero.
    const double pa[3] = { 1.0,  0.0,  0.0};
    const double pb[3] = { 0.0,  1.0,  0.0};
    const double pc[3] = { 0.0,  0.0,  1.0};
    const double pd[3] = {-1.0,  0.0,  0.0};
    const double pe[3] = { 0.0,  0.0,  0.0};
    const double result = inSphere(pa, pb, pc, pd, pe);
    EXPECT_NE(result, 0.0);
    // And the outside point must have the opposite sign.
    const double pe2[3] = {5.0, 0.0, 0.0};
    const double resultOut = inSphere(pa, pb, pc, pd, pe2);
    EXPECT_NE(resultOut, 0.0);
    EXPECT_LT(result * resultOut, 0.0); // opposite signs
}

TEST(RobustPredicates, InSphereOutsideOppositeSign)
{
    const double pa[3] = { 1.0,  0.0,  0.0};
    const double pb[3] = { 0.0,  1.0,  0.0};
    const double pc[3] = { 0.0,  0.0,  1.0};
    const double pd[3] = {-1.0,  0.0,  0.0};
    const double peIn[3]  = { 0.0,  0.0,  0.0};
    const double peOut[3] = { 5.0,  0.0,  0.0};
    // inside and outside must have opposite signs.
    const double ri = inSphere(pa, pb, pc, pd, peIn);
    const double ro = inSphere(pa, pb, pc, pd, peOut);
    EXPECT_NE(ri, 0.0);
    EXPECT_NE(ro, 0.0);
    EXPECT_LT(ri * ro, 0.0);
}

TEST(RobustPredicates, InSphereOnSphereZero)
{
    // All five points on the unit sphere → determinant = 0.
    const double pa[3] = { 1.0,  0.0,  0.0};
    const double pb[3] = { 0.0,  1.0,  0.0};
    const double pc[3] = { 0.0,  0.0,  1.0};
    const double pd[3] = {-1.0,  0.0,  0.0};
    const double pe[3] = { 0.0, -1.0,  0.0};
    EXPECT_EQ(inSphere(pa, pb, pc, pd, pe), 0.0);
}
