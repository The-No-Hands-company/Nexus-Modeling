#include <nexus/geometry/ConstrainedDelaunay2D.h>
#include <gtest/gtest.h>
#include <algorithm>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static bool hasEdge(const std::vector<DelaunayTriangle>& tris, uint32_t a, uint32_t b)
{
    for (const auto& t : tris) {
        const uint32_t v[3] = {t.a, t.b, t.c};
        for (int i = 0; i < 3; ++i) {
            if ((v[i] == a && v[(i+1)%3] == b) || (v[i] == b && v[(i+1)%3] == a))
                return true;
        }
    }
    return false;
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(ConstrainedDelaunay2D, EmptyYieldsNoTriangles)
{
    ConstrainedDelaunay2D cdt;
    cdt.triangulate({}, {});
    EXPECT_TRUE(cdt.triangles().empty());
    EXPECT_TRUE(cdt.points().empty());
}

TEST(ConstrainedDelaunay2D, ThreePointsOneTriangle)
{
    ConstrainedDelaunay2D cdt;
    cdt.triangulate({{0,0},{1,0},{0,1}}, {});
    EXPECT_EQ(cdt.triangles().size(), 1u);
}

TEST(ConstrainedDelaunay2D, IsConstrainedReturnsTrueForAddedEdge)
{
    ConstrainedDelaunay2D cdt;
    cdt.triangulate({{0,0},{1,0},{1,1},{0,1}}, {{0,2}});
    EXPECT_TRUE(cdt.isConstrained(0, 2));
    EXPECT_TRUE(cdt.isConstrained(2, 0)); // symmetric
}

TEST(ConstrainedDelaunay2D, IsConstrainedReturnsFalseForUnaddedEdge)
{
    ConstrainedDelaunay2D cdt;
    cdt.triangulate({{0,0},{1,0},{1,1},{0,1}}, {{0,2}});
    EXPECT_FALSE(cdt.isConstrained(0, 1));
    EXPECT_FALSE(cdt.isConstrained(1, 3));
}

TEST(ConstrainedDelaunay2D, ConstraintEdgeAlreadyPresentInTriangulation)
{
    // Square: Delaunay will produce diagonal 0-2 or 1-3.
    // Force 0-2 as constraint; edgeExists should be true.
    ConstrainedDelaunay2D cdt;
    cdt.triangulate({{0,0},{2,0},{1,1},{-1,1}}, {{0,2}});
    // After constraint enforcement, 0-2 must be present.
    EXPECT_TRUE(cdt.edgeExists(0, 2));
}

TEST(ConstrainedDelaunay2D, FourPointsYieldsTwoTriangles)
{
    ConstrainedDelaunay2D cdt;
    cdt.triangulate({{0,0},{1,0},{1,1},{0,1}}, {});
    EXPECT_EQ(cdt.triangles().size(), 2u);
}

TEST(ConstrainedDelaunay2D, AddConstraintBeforeTriangulation)
{
    ConstrainedDelaunay2D cdt;
    cdt.addConstraint(0, 2);
    EXPECT_TRUE(cdt.isConstrained(0, 2));
    EXPECT_EQ(cdt.constraints().size(), 1u);
}

TEST(ConstrainedDelaunay2D, NoSelfLoopConstraint)
{
    ConstrainedDelaunay2D cdt;
    cdt.addConstraint(3, 3); // self-loop, should be ignored
    EXPECT_EQ(cdt.constraints().size(), 0u);
}

TEST(ConstrainedDelaunay2D, EdgeExistsForTriangulationEdge)
{
    ConstrainedDelaunay2D cdt;
    cdt.triangulate({{0,0},{1,0},{0.5,1}}, {});
    // All three edges of the single triangle must exist.
    EXPECT_TRUE(cdt.edgeExists(0, 1));
    EXPECT_TRUE(cdt.edgeExists(1, 2));
    EXPECT_TRUE(cdt.edgeExists(2, 0));
}

TEST(ConstrainedDelaunay2D, ToMeshHasCorrectVertexCount)
{
    ConstrainedDelaunay2D cdt;
    cdt.triangulate({{0,0},{1,0},{1,1},{0,1}}, {});
    auto m = cdt.toMesh();
    EXPECT_EQ(m.attributes().positions().size(), 4u);
}
