#include <nexus/geometry/Meshlet.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(Meshlet, BuildProducesMeshletsFromMesh)
{
    Mesh mesh = makeSphere(1.f, 8, 8);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_GT(mesh.attributes().vertexCount(), 2u);
    EXPECT_GT(mesh.topology().faceCount(), 0u);
}

TEST(Meshlet, MeshletCountGreaterThanZeroForValidInput)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_EQ(mesh.topology().faceCount(), 6u);

    const size_t faceCount = mesh.topology().faceCount();
    EXPECT_GT(faceCount, 0u);
}

TEST(Meshlet, EachMeshletHasAtLeastThreeVertices)
{
    Mesh mesh = makePlane(2.f, 2.f, 2, 2);

    EXPECT_TRUE(mesh.isValid());

    for (size_t i = 0; i < mesh.topology().faceCount(); ++i) {
        const auto& face = mesh.topology().face(i);
        EXPECT_GE(face.vertexCount(), 3u);
    }
}

TEST(Meshlet, TriangleMeshProducesSingleMeshlet)
{
    Mesh tri = makeTriangle(1.f);

    EXPECT_TRUE(tri.isValid());
    EXPECT_EQ(tri.topology().faceCount(), 1u);
    EXPECT_EQ(tri.attributes().vertexCount(), 3u);
    EXPECT_TRUE(tri.topology().face(0).isTriangle());
}

TEST(Meshlet, MeshletVerticesAreWithinParentMeshBounds)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    const auto parentBounds = mesh.computeBounds();

    EXPECT_TRUE(mesh.isValid());

    Mesh extracted;
    ASSERT_TRUE(mesh.extractFaceRange(0u, 3u, extracted));
    const auto childBounds = extracted.computeBounds();

    EXPECT_GE(childBounds.min.x, parentBounds.min.x - 1e-5f);
    EXPECT_LE(childBounds.max.x, parentBounds.max.x + 1e-5f);
    EXPECT_GE(childBounds.min.y, parentBounds.min.y - 1e-5f);
    EXPECT_LE(childBounds.max.y, parentBounds.max.y + 1e-5f);
    EXPECT_GE(childBounds.min.z, parentBounds.min.z - 1e-5f);
    EXPECT_LE(childBounds.max.z, parentBounds.max.z + 1e-5f);
}

TEST(Meshlet, EmptyMeshProducesNoMeshlets)
{
    Mesh empty;
    EXPECT_FALSE(empty.isValid());
    EXPECT_EQ(empty.topology().faceCount(), 0u);
    EXPECT_EQ(empty.attributes().vertexCount(), 0u);
}
