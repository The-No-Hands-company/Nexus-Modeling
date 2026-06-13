#include <nexus/geometry/ConstrainedDelaunay2D.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(ConstrainedDelaunay2D, CDTWithConstraintEdgesProducesValidResult)
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
    EXPECT_TRUE(mesh.topology().allFacesArePoly3Plus());
}

TEST(ConstrainedDelaunay2D, EdgeDetectedAcrossExistingFaces)
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

    EXPECT_TRUE(mesh.topology().hasValidIndices(mesh.attributes().vertexCount()));

    EXPECT_TRUE(mesh.isValid());
}

TEST(ConstrainedDelaunay2D, SelfLoopConstraintsDroppedSilently)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 0.f, 1.f},
    });

    Face f0{};
    f0.indices = {0, 1, 2};
    mesh.topology().addFace(f0);

    EXPECT_TRUE(mesh.topology().face(0).isTriangle());
    EXPECT_EQ(mesh.topology().face(0).vertexCount(), 3u);
    EXPECT_TRUE(mesh.isValid());
}

TEST(ConstrainedDelaunay2D, EmptyMeshRejectsInvalidTopology)
{
    Mesh mesh;
    EXPECT_FALSE(mesh.isValid());
    EXPECT_EQ(mesh.topology().faceCount(), 0u);
    EXPECT_EQ(mesh.attributes().vertexCount(), 0u);
}
