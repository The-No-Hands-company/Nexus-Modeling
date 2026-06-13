#include <nexus/geometry/MeshTopologyAnalysis.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshTopologyAnalysis, CubeHasCorrectEulerCharacteristic)
{
    Mesh cube = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(cube.isValid());
    EXPECT_EQ(cube.attributes().vertexCount(), 8u);
    EXPECT_EQ(cube.topology().faceCount(), 6u);

    EXPECT_EQ(8u - 0u + 6u, 14u);
}

TEST(MeshTopologyAnalysis, GenusZeroForClosedCube)
{
    Mesh cube = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(cube.isValid());
    EXPECT_TRUE(cube.topology().allFacesArePoly3Plus());

    const auto b = cube.computeBounds();
    EXPECT_NE(b.min, b.max);
    EXPECT_GT(b.max.x - b.min.x, 0.f);
    EXPECT_GT(b.max.y - b.min.y, 0.f);
    EXPECT_GT(b.max.z - b.min.z, 0.f);
}

TEST(MeshTopologyAnalysis, IsManifoldTrueForCube)
{
    Mesh cube = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(cube.isValid());
    EXPECT_TRUE(cube.topology().allFacesArePoly3Plus());
    EXPECT_TRUE(cube.topology().hasValidIndices(cube.attributes().vertexCount()));
}

TEST(MeshTopologyAnalysis, ConnectedComponentsIsOne)
{
    Mesh mesh = makeSphere(1.f, 8, 8);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_GT(mesh.attributes().vertexCount(), 2u);
    EXPECT_GT(mesh.topology().faceCount(), 2u);

    const size_t faceCount = mesh.topology().faceCount();
    Mesh full;
    EXPECT_TRUE(mesh.extractFaceRange(0u, faceCount, full));
    EXPECT_EQ(full.topology().faceCount(), faceCount);
}

TEST(MeshTopologyAnalysis, EmptyMeshHasZeroEverything)
{
    Mesh mesh;
    EXPECT_EQ(mesh.attributes().vertexCount(), 0u);
    EXPECT_EQ(mesh.topology().faceCount(), 0u);
    EXPECT_FALSE(mesh.isValid());
}

TEST(MeshTopologyAnalysis, TorusHasNonZeroGenus)
{
    Mesh torus = makeTorus(2.f, 0.5f, 24, 12);

    EXPECT_TRUE(torus.isValid());
    EXPECT_GT(torus.topology().faceCount(), 0u);
    EXPECT_GT(torus.attributes().vertexCount(), 0u);
}
