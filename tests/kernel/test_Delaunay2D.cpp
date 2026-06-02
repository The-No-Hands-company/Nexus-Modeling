#include <nexus/geometry/Delaunay2D.h>
#include <nexus/geometry/RobustPredicates.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace nexus::geometry;

// Verify the Delaunay criterion: for every triangle T, no other point p
// lies strictly inside the circumcircle of T.
static bool checkDelaunayCriterion(const Delaunay2D& dt)
{
    const auto& pts = dt.points();
    const auto& tris = dt.triangles();
    for (const auto& tri : tris) {
        const double pa[2] = {pts[tri.a].x, pts[tri.a].y};
        const double pb[2] = {pts[tri.b].x, pts[tri.b].y};
        const double pc[2] = {pts[tri.c].x, pts[tri.c].y};
        for (uint32_t k = 0; k < static_cast<uint32_t>(pts.size()); ++k) {
            if (k == tri.a || k == tri.b || k == tri.c) { continue; }
            const double pd[2] = {pts[k].x, pts[k].y};
            if (nexus::geometry::predicates::inCircle(pa, pb, pc, pd) > 1e-10) {
                return false; // point strictly inside circumcircle
            }
        }
    }
    return true;
}

// ─── Construction tests ───────────────────────────────────────────────────────

TEST(Delaunay2D, EmptyInputProducesNoTriangles)
{
    Delaunay2D dt;
    dt.triangulate({});
    EXPECT_EQ(dt.triangles().size(), 0u);
    EXPECT_EQ(dt.points().size(), 0u);
}

TEST(Delaunay2D, ThreePointsProducesOneTriangle)
{
    Delaunay2D dt;
    dt.triangulate({{0,0},{1,0},{0,1}});
    EXPECT_EQ(dt.triangles().size(), 1u);
    EXPECT_EQ(dt.points().size(), 3u);
}

TEST(Delaunay2D, FourPointsProducesTwoTriangles)
{
    Delaunay2D dt;
    dt.triangulate({{0,0},{1,0},{1,1},{0,1}});
    EXPECT_EQ(dt.triangles().size(), 2u);
}

TEST(Delaunay2D, TriangleIndicesInBounds)
{
    Delaunay2D dt;
    dt.triangulate({{0,0},{2,0},{1,2},{3,1}});
    const uint32_t n = static_cast<uint32_t>(dt.points().size());
    for (const auto& tri : dt.triangles()) {
        EXPECT_LT(tri.a, n);
        EXPECT_LT(tri.b, n);
        EXPECT_LT(tri.c, n);
        // All indices must be distinct.
        EXPECT_NE(tri.a, tri.b);
        EXPECT_NE(tri.b, tri.c);
        EXPECT_NE(tri.a, tri.c);
    }
}

TEST(Delaunay2D, EulerFormulaForPlanarGraph)
{
    // For a convex point set with n points, Delaunay triangulates to
    // 2n-2-h triangles (h = hull edges, typically 2n-5 interior + hull).
    // Basic check: n points → at least n-2 triangles (lower bound).
    Delaunay2D dt;
    dt.triangulate({{0,0},{1,0},{2,0.1},{1.5,1},{0.5,1.2},{-0.2,0.7}});
    const size_t n = dt.points().size();
    EXPECT_GE(dt.triangles().size(), n - 2);
}

// ─── Delaunay criterion ───────────────────────────────────────────────────────

TEST(Delaunay2D, DelaunayCriterionFourPoints)
{
    Delaunay2D dt;
    dt.triangulate({{0,0},{1,0},{1,1},{0,1}});
    EXPECT_TRUE(checkDelaunayCriterion(dt));
}

TEST(Delaunay2D, DelaunayCriterionFivePoints)
{
    Delaunay2D dt;
    dt.triangulate({{0,0},{2,0},{2,2},{0,2},{1,1}});
    EXPECT_TRUE(checkDelaunayCriterion(dt));
}

TEST(Delaunay2D, DelaunayCriterionRandomGrid)
{
    Delaunay2D dt;
    std::vector<Point2D> pts;
    // 3×3 grid with slight jitter to avoid degeneracies.
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            pts.push_back({i + 0.01 * (i + j), j + 0.01 * (j - i)});
        }
    }
    dt.triangulate(pts);
    EXPECT_TRUE(checkDelaunayCriterion(dt));
}

// ─── Export ───────────────────────────────────────────────────────────────────

TEST(Delaunay2D, ToMeshHasCorrectCounts)
{
    Delaunay2D dt;
    dt.triangulate({{0,0},{1,0},{0,1},{1,1}});
    const auto mesh = dt.toMesh();
    EXPECT_EQ(mesh.attributes().vertexCount(), dt.points().size());
    EXPECT_EQ(mesh.topology().faceCount(), dt.triangles().size());
}

// ─── Incremental insert ───────────────────────────────────────────────────────

TEST(Delaunay2D, IncrementalInsertCoincidentReturnsExisting)
{
    Delaunay2D dt;
    const uint32_t a = dt.insert(0.0, 0.0);
    const uint32_t b = dt.insert(0.0, 0.0); // coincident
    EXPECT_EQ(a, b);
}

TEST(Delaunay2D, ClearResetsState)
{
    Delaunay2D dt;
    dt.triangulate({{0,0},{1,0},{0,1}});
    dt.clear();
    EXPECT_EQ(dt.points().size(), 0u);
    EXPECT_EQ(dt.triangles().size(), 0u);
}
