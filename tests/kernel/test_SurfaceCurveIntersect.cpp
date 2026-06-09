#include <nexus/geometry/SurfaceCurveIntersect.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Flat plane at z=0, covering [0,1]×[0,1].
static NurbsSurface makeUnitPlane()
{
    std::vector<nexus::render::Vec3> pts = {
        {0,0,0},{1,0,0},{0,1,0},{1,1,0},
    };
    return *NurbsSurface::fromGrid(pts, 2, 2, 1, 1);
}

// Straight vertical line piercing the plane at (0.5, 0.5, z) — z from -1 to 1.
static NurbsCurve makeVerticalPiercingLine()
{
    std::vector<nexus::render::Vec3> pts = {
        {0.5f, 0.5f, -1.f},
        {0.5f, 0.5f,  0.f},
        {0.5f, 0.5f,  1.f},
    };
    return *NurbsCurve::fromControlPoints(pts, 2);
}

// Straight line entirely above the plane: z from 1 to 2, does NOT intersect.
static NurbsCurve makeNonIntersectingLine()
{
    std::vector<nexus::render::Vec3> pts = {
        {0.5f, 0.5f, 1.f},
        {0.5f, 0.5f, 2.f},
    };
    return *NurbsCurve::fromControlPoints(pts, 1);
}

// ─── Basic intersection tests ─────────────────────────────────────────────────

TEST(SurfaceCurveIntersect, PiercingLineFindsOneHit)
{
    auto surf  = makeUnitPlane();
    auto curve = makeVerticalPiercingLine();
    SurfaceCurveIsectOptions opts;
    opts.gridT = 16; opts.gridU = 16; opts.gridV = 16;
    opts.tolerance = 1e-4f;
    auto hits = SurfaceCurveIntersector::intersect(curve, surf, opts);
    EXPECT_GE(hits.size(), 1u);
}

TEST(SurfaceCurveIntersect, PiercingLineHitNearMidpoint)
{
    auto surf  = makeUnitPlane();
    auto curve = makeVerticalPiercingLine();
    SurfaceCurveIsectOptions opts;
    opts.gridT = 16; opts.gridU = 16; opts.gridV = 16;
    opts.tolerance = 1e-4f;
    auto hits = SurfaceCurveIntersector::intersect(curve, surf, opts);
    ASSERT_GE(hits.size(), 1u);
    // Intersection should be near z=0 (plane) and x=0.5, y=0.5.
    EXPECT_NEAR(hits[0].point.z, 0.f, 0.05f);
    EXPECT_NEAR(hits[0].point.x, 0.5f, 0.2f);
    EXPECT_NEAR(hits[0].point.y, 0.5f, 0.2f);
}

TEST(SurfaceCurveIntersect, NonIntersectingLineFindsNoHit)
{
    auto surf  = makeUnitPlane();
    auto curve = makeNonIntersectingLine();
    SurfaceCurveIsectOptions opts;
    opts.gridT = 8; opts.gridU = 8; opts.gridV = 8;
    opts.tolerance = 1e-4f;
    auto hits = SurfaceCurveIntersector::intersect(curve, surf, opts);
    EXPECT_EQ(hits.size(), 0u);
}

TEST(SurfaceCurveIntersect, ResultsSortedByT)
{
    auto surf  = makeUnitPlane();
    auto curve = makeVerticalPiercingLine();
    SurfaceCurveIsectOptions opts;
    opts.gridT = 16; opts.gridU = 16; opts.gridV = 16;
    opts.tolerance = 1e-4f;
    auto hits = SurfaceCurveIntersector::intersect(curve, surf, opts);
    for (size_t i = 1; i < hits.size(); ++i) {
        EXPECT_LE(hits[i-1].t, hits[i].t);
    }
}

TEST(SurfaceCurveIntersect, HitParametersWithinBounds)
{
    auto surf  = makeUnitPlane();
    auto curve = makeVerticalPiercingLine();
    SurfaceCurveIsectOptions opts;
    opts.tolerance = 1e-4f;
    auto hits = SurfaceCurveIntersector::intersect(curve, surf, opts);
    for (const auto& h : hits) {
        EXPECT_GE(h.t, curve.paramMin());
        EXPECT_LE(h.t, curve.paramMax());
        EXPECT_GE(h.u, surf.paramMinU());
        EXPECT_LE(h.u, surf.paramMaxU());
        EXPECT_GE(h.v, surf.paramMinV());
        EXPECT_LE(h.v, surf.paramMaxV());
    }
}

TEST(SurfaceCurveIntersect, EmptyResultForLineParallelToPlane)
{
    auto surf = makeUnitPlane();
    // Horizontal line at z=2 (far above surface, no intersection).
    std::vector<nexus::render::Vec3> pts = {{0.f,0.f,2.f},{1.f,1.f,2.f}};
    auto curve = *NurbsCurve::fromControlPoints(pts, 1);
    SurfaceCurveIsectOptions opts;
    opts.gridT = 8; opts.gridU = 8; opts.gridV = 8;
    opts.tolerance = 1e-4f;
    auto hits = SurfaceCurveIntersector::intersect(curve, surf, opts);
    EXPECT_EQ(hits.size(), 0u);
}

TEST(SurfaceCurveIntersect, CornerPiercingLineDetected)
{
    auto surf = makeUnitPlane();
    // Line piercing near corner (0.1, 0.1).
    std::vector<nexus::render::Vec3> pts = {{0.1f,0.1f,-0.5f},{0.1f,0.1f,0.5f}};
    auto curve = *NurbsCurve::fromControlPoints(pts, 1);
    SurfaceCurveIsectOptions opts;
    opts.gridT = 8; opts.gridU = 16; opts.gridV = 16;
    opts.tolerance = 1e-4f;
    auto hits = SurfaceCurveIntersector::intersect(curve, surf, opts);
    EXPECT_GE(hits.size(), 1u);
}
