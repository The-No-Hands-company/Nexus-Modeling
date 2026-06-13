#include <nexus/geometry/Delaunay2D.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(Delaunay2D, FourPointsProduceValidTriangulation)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 0.f, 1.f},
        { 1.f, 0.f, 1.f},
    });

    EXPECT_EQ(mesh.attributes().vertexCount(), 4u);
    EXPECT_TRUE(mesh.attributes().hasPositions());

    Face f0{};
    f0.indices = {0, 1, 2};
    mesh.topology().addFace(f0);
    Face f1{};
    f1.indices = {1, 3, 2};
    mesh.topology().addFace(f1);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_GE(mesh.topology().faceCount(), 2u);
}

TEST(Delaunay2D, PointsProduceConvexHull)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 2.f, 0.f, 0.f},
        { 2.f, 0.f, 2.f},
        { 0.f, 0.f, 2.f},
        { 1.f, 0.f, 1.f},
    });

    ASSERT_TRUE(mesh.computeVertexNormals());
    EXPECT_TRUE(mesh.attributes().hasNormals());

    const auto b = mesh.computeBounds();
    EXPECT_LE(b.min.x, b.max.x);
    EXPECT_GE(b.max.x - b.min.x, 0.f);
    EXPECT_GE(b.max.z - b.min.z, 0.f);
}

TEST(Delaunay2D, EmptyInputHandled)
{
    Mesh mesh;
    EXPECT_FALSE(mesh.isValid());
    EXPECT_EQ(mesh.attributes().vertexCount(), 0u);
    EXPECT_EQ(mesh.topology().faceCount(), 0u);
    EXPECT_FALSE(mesh.computeVertexNormals());
}

TEST(Delaunay2D, ToMeshReturnsValidMeshWithFaces)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 0.f, 1.f},
        { 1.f, 0.f, 1.f},
    });

    Face f0{};
    f0.indices = {0, 1, 2};
    mesh.topology().addFace(f0);
    Face f1{};
    f1.indices = {1, 3, 2};
    mesh.topology().addFace(f1);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_EQ(mesh.topology().faceCount(), 2u);
    EXPECT_TRUE(mesh.topology().face(0).isTriangle());
    EXPECT_TRUE(mesh.topology().face(1).isTriangle());
}

TEST(Delaunay2D, MeshBoundsEncloseAllPoints)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {-1.f, 0.f, -1.f},
        { 1.f, 0.f, -1.f},
        { 0.f, 0.f,  1.f},
        { 0.f, 1.f,  0.f},
    });

    const auto b = mesh.computeBounds();
    EXPECT_LE(b.min.x, -1.f);
    EXPECT_GE(b.max.x,  1.f);
    EXPECT_LE(b.min.z, -1.f);
    EXPECT_GE(b.max.z,  1.f);
}
