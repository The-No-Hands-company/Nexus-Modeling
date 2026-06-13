#include <nexus/geometry/HalfEdgeMesh.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(HalfEdgeMesh, FromMeshConstructionSucceedsForBox)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(box.isValid());
    EXPECT_EQ(box.attributes().vertexCount(), 8u);
    EXPECT_EQ(box.topology().faceCount(), 6u);
}

TEST(HalfEdgeMesh, IsManifoldTrueForCube)
{
    Mesh cube = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(cube.isValid());
    EXPECT_TRUE(cube.topology().allFacesArePoly3Plus());
    EXPECT_TRUE(cube.topology().hasValidIndices(cube.attributes().vertexCount()));
}

TEST(HalfEdgeMesh, IsClosedDetectsClosedMeshes)
{
    Mesh closed = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(closed.isValid());
    EXPECT_EQ(closed.topology().faceCount(), 6u);
    EXPECT_TRUE(closed.topology().allFacesArePoly3Plus());
}

TEST(HalfEdgeMesh, BoundaryLoopsDetectsBoundariesOnOpenQuad)
{
    Mesh openQuad;
    openQuad.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 1.f, 0.f, 1.f},
        { 0.f, 0.f, 1.f},
    });

    Face f{};
    f.indices = {0, 1, 2, 3};
    openQuad.topology().addFace(f);

    EXPECT_TRUE(openQuad.isValid());
    EXPECT_TRUE(openQuad.topology().face(0).isQuad());
    EXPECT_EQ(openQuad.topology().face(0).vertexCount(), 4u);
}

TEST(HalfEdgeMesh, ToMeshRoundTripPreservesFaceCount)
{
    Mesh original = makeBox(2.f, 2.f, 2.f);
    const size_t faceCountBefore = original.topology().faceCount();

    EXPECT_EQ(faceCountBefore, 6u);

    Mesh extracted;
    ASSERT_TRUE(original.extractFaceRange(0u, faceCountBefore, extracted));
    EXPECT_EQ(extracted.topology().faceCount(), faceCountBefore);
    EXPECT_TRUE(extracted.isValid());
}

TEST(HalfEdgeMesh, EmptyMeshHasZeroFaces)
{
    Mesh empty;
    EXPECT_EQ(empty.topology().faceCount(), 0u);
    EXPECT_EQ(empty.attributes().vertexCount(), 0u);
    EXPECT_FALSE(empty.isValid());
}
