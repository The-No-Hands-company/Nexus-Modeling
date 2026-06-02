#include <nexus/geometry/Voronoi2D.h>
#include <nexus/geometry/Delaunay2D.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static VoronoiDiagram makeDiagram(const std::vector<Point2D>& pts)
{
    Delaunay2D dt;
    dt.triangulate(pts);
    return Voronoi2D::fromDelaunay(dt);
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(Voronoi2D, EmptyInputYieldsEmptyDiagram)
{
    Delaunay2D dt;
    auto vd = Voronoi2D::fromDelaunay(dt);
    EXPECT_EQ(vd.vertexCount(), 0u);
    EXPECT_EQ(vd.edgeCount(),   0u);
    EXPECT_EQ(vd.cellCount(),   0u);
}

TEST(Voronoi2D, ThreePointsOneVoronoiVertex)
{
    // Three input points → 1 Delaunay triangle → 1 Voronoi vertex.
    auto vd = makeDiagram({{0,0},{1,0},{0,1}});
    EXPECT_EQ(vd.vertexCount(), 1u);
}

TEST(Voronoi2D, ThreePointsThreeEdges)
{
    // Three input points → 3 Delaunay edges → 3 Voronoi edges (all boundary rays).
    auto vd = makeDiagram({{0,0},{1,0},{0,1}});
    EXPECT_EQ(vd.edgeCount(), 3u);
}

TEST(Voronoi2D, CellCountMatchesSiteCount)
{
    auto vd = makeDiagram({{0,0},{2,0},{2,2},{0,2},{1,1}});
    EXPECT_EQ(vd.cellCount(), vd.cells().size());
    EXPECT_EQ(vd.cellCount(), 5u);
}

TEST(Voronoi2D, FourPointsTwoVoronoiVertices)
{
    // 4 sites → 2 Delaunay triangles → 2 Voronoi vertices.
    auto vd = makeDiagram({{0,0},{1,0},{1,1},{0,1}});
    EXPECT_EQ(vd.vertexCount(), 2u);
}

TEST(Voronoi2D, FiniteEdgeConnectsTwoVoronoiVertices)
{
    // 4 sites in a convex quad → 1 shared interior Delaunay edge → 1 finite Voronoi edge.
    auto vd = makeDiagram({{0,0},{1,0},{1,1},{0,1}});
    uint32_t finiteCount = 0;
    for (const auto& e : vd.edges()) {
        if (e.isFinite) { ++finiteCount; }
    }
    EXPECT_GE(finiteCount, 1u);
}

TEST(Voronoi2D, CircumcenterOfEquilateralTriangle)
{
    // Equilateral triangle: circumcenter is at centroid.
    const double h = std::sqrt(3.0) / 2.0;
    auto vd = makeDiagram({{0.0, 0.0}, {1.0, 0.0}, {0.5, h}});
    ASSERT_EQ(vd.vertexCount(), 1u);
    const auto& cv = vd.vertices()[0];
    EXPECT_NEAR(cv.x, 0.5, 1e-9);
    EXPECT_NEAR(cv.y, h / 3.0, 1e-9);
}

TEST(Voronoi2D, SiteIndicesOnEdgeAreValid)
{
    auto vd = makeDiagram({{0,0},{1,0},{0.5,1}});
    for (const auto& e : vd.edges()) {
        EXPECT_LT(e.site0, vd.cellCount());
        EXPECT_LT(e.site1, vd.cellCount());
    }
}

TEST(Voronoi2D, CellEdgeIndicesInRange)
{
    auto vd = makeDiagram({{0,0},{1,0},{1,1},{0,1},{0.5,0.5}});
    for (const auto& cell : vd.cells()) {
        for (uint32_t ei : cell.edgeIndices) {
            EXPECT_LT(ei, vd.edgeCount());
        }
    }
}

TEST(Voronoi2D, VoronoiVerticesAreCircumcenters)
{
    // For each Voronoi vertex (circumcenter of Delaunay triangle ti),
    // its distance to all three triangle vertices must be equal.
    Delaunay2D dt;
    dt.triangulate({{0,0},{3,0},{3,3},{0,3},{1.5,1.5}});
    auto vd = Voronoi2D::fromDelaunay(dt);
    const auto& pts  = dt.points();
    const auto& tris = dt.triangles();
    ASSERT_EQ(vd.vertexCount(), tris.size());
    for (uint32_t ti = 0; ti < static_cast<uint32_t>(tris.size()); ++ti) {
        const auto& cv = vd.vertices()[ti];
        const auto& t  = tris[ti];
        auto dist2 = [&](uint32_t v) {
            double dx = pts[v].x - cv.x, dy = pts[v].y - cv.y;
            return dx*dx + dy*dy;
        };
        EXPECT_NEAR(dist2(t.a), dist2(t.b), 1e-6);
        EXPECT_NEAR(dist2(t.b), dist2(t.c), 1e-6);
    }
}
