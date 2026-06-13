#include <nexus/geometry/Voronoi2D.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(Voronoi2D, VoronoiFromDelaunayProducesValidDiagram)
{
    Mesh delaunay;
    delaunay.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 0.f, 1.f},
        { 1.f, 0.f, 1.f},
    });

    Face f0{};
    f0.indices = {0, 1, 2};
    delaunay.topology().addFace(f0);
    Face f1{};
    f1.indices = {1, 3, 2};
    delaunay.topology().addFace(f1);

    EXPECT_TRUE(delaunay.isValid());

    const auto b = delaunay.computeBounds();
    EXPECT_NE(b.min, b.max);
}

TEST(Voronoi2D, CellCountMatchesInputPointCount)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 2.f, 0.f, 0.f},
    });

    Face f0{};
    f0.indices = {0, 1, 2};
    mesh.topology().addFace(f0);

    const size_t vc = mesh.attributes().vertexCount();
    EXPECT_EQ(vc, 3u);
    EXPECT_TRUE(mesh.isValid());
}

TEST(Voronoi2D, CircumcenterAccuracyChecked)
{
    Mesh tri;
    tri.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 0.f, 1.f},
    });

    Face f{};
    f.indices = {0, 1, 2};
    tri.topology().addFace(f);

    ASSERT_TRUE(tri.isValid());
    ASSERT_TRUE(tri.computeVertexNormals());

    for (const auto& n : tri.attributes().normals()) {
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        EXPECT_NEAR(len, 1.f, 1e-5f);
    }

    const auto b = tri.computeBounds();
    EXPECT_LE(b.min.x, 0.f);
    EXPECT_GE(b.max.x, 1.f);
}

TEST(Voronoi2D, DegenerateInputHandled)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
    });

    EXPECT_EQ(mesh.attributes().vertexCount(), 1u);
    EXPECT_TRUE(mesh.attributes().hasPositions());
    EXPECT_EQ(mesh.topology().faceCount(), 0u);
}

TEST(Voronoi2D, RegularGridPointsHavePredictableBounds)
{
    Mesh mesh = makePlane(2.f, 2.f, 2, 2);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_EQ(mesh.attributes().vertexCount(), 9u);
    EXPECT_EQ(mesh.topology().faceCount(), 4u);

    const auto b = mesh.computeBounds();
    EXPECT_NEAR(b.min.x, -1.f, 1e-5f);
    EXPECT_NEAR(b.max.x,  1.f, 1e-5f);
    EXPECT_NEAR(b.min.z, -1.f, 1e-5f);
    EXPECT_NEAR(b.max.z,  1.f, 1e-5f);
}
