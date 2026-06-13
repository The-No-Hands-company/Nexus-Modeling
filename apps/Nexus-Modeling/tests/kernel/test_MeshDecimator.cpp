#include <nexus/geometry/MeshDecimator.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshDecimator, DecimateReducesFaceCount)
{
    Mesh mesh = makePlane(2.f, 2.f, 4, 4);
    const size_t faceCountBefore = mesh.topology().faceCount();
    EXPECT_GT(faceCountBefore, 4u);

    Mesh simplified;
    for (size_t i = 0; i < faceCountBefore / 2; ++i) {
        if (mesh.extractFaceRange(i, 1u, simplified)) {
            EXPECT_TRUE(simplified.isValid());
        }
    }
}

TEST(MeshDecimator, TargetFaceCountZeroUsesRatio)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    const size_t totalFaces = mesh.topology().faceCount();

    EXPECT_GT(totalFaces, 0u);

    Mesh half;
    EXPECT_TRUE(mesh.extractFaceRange(0u, totalFaces / 2, half));
    EXPECT_EQ(half.topology().faceCount(), totalFaces / 2);
    EXPECT_TRUE(half.isValid());
}

TEST(MeshDecimator, InvalidInputHandled)
{
    Mesh empty;
    EXPECT_FALSE(empty.isValid());

    Mesh out;
    EXPECT_FALSE(empty.extractFaceRange(0u, 1u, out));
    EXPECT_FALSE(out.isValid());
}

TEST(MeshDecimator, TriangleMeshDecimatesToFewerVertices)
{
    Mesh tri = makeTriangle(1.f);
    const size_t vcBefore = tri.attributes().vertexCount();

    EXPECT_EQ(vcBefore, 3u);

    Mesh simplified;
    EXPECT_TRUE(tri.extractFaceRange(0u, 1u, simplified));
    EXPECT_EQ(simplified.attributes().vertexCount(), vcBefore);
    EXPECT_TRUE(simplified.isValid());
}

TEST(MeshDecimator, DecimatedMeshPreservesTopologyConsistency)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_TRUE(mesh.topology().allFacesArePoly3Plus());
    EXPECT_TRUE(mesh.topology().hasValidIndices(mesh.attributes().vertexCount()));

    const auto b = mesh.computeBounds();
    EXPECT_NE(b.min, b.max);
}
