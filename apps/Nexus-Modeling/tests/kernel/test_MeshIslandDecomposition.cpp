#include <gtest/gtest.h>

#include <nexus/geometry/MeshIslandDecomposition.h>
#include <nexus/geometry/Mesh.h>

namespace {

using namespace nexus::geometry;

static Mesh makeTwoDisconnectedQuads()
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},   // 0 - quad1
        {1.f, 0.f, 0.f},   // 1
        {1.f, 0.f, 1.f},   // 2
        {0.f, 0.f, 1.f},   // 3
        {5.f, 0.f, 0.f},   // 4 - quad2 (far away)
        {6.f, 0.f, 0.f},   // 5
        {6.f, 0.f, 1.f},   // 6
        {5.f, 0.f, 1.f},   // 7
    });
    mesh.topology().addFace({{0, 1, 2, 3}});
    mesh.topology().addFace({{4, 5, 6, 7}});
    return mesh;
}

static Mesh makeSingleQuad()
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
    });
    mesh.topology().addFace({{0, 1, 2, 3}});
    return mesh;
}

static Mesh makeTwoConnectedTriangles()
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {1.f, 1.f, 0.f},
    });
    mesh.topology().addFace({{0, 1, 2}});
    mesh.topology().addFace({{1, 3, 2}});
    return mesh;
}

TEST(MeshIslandDecomposition, TwoDisconnectedQuadsTwoIslands)
{
    auto mesh = makeTwoDisconnectedQuads();
    ASSERT_TRUE(mesh.isValid());
    auto islands = MeshIslandDecomposition::decompose(mesh);
    EXPECT_EQ(islands.size(), 2u);
}

TEST(MeshIslandDecomposition, SingleQuadOneIsland)
{
    auto mesh = makeSingleQuad();
    ASSERT_TRUE(mesh.isValid());
    auto islands = MeshIslandDecomposition::decompose(mesh);
    EXPECT_EQ(islands.size(), 1u);
}

TEST(MeshIslandDecomposition, CountMatchesDecomposeSize)
{
    auto mesh = makeTwoDisconnectedQuads();
    ASSERT_TRUE(mesh.isValid());
    size_t cnt = MeshIslandDecomposition::count(mesh);
    auto islands = MeshIslandDecomposition::decompose(mesh);
    EXPECT_EQ(cnt, islands.size());
}

TEST(MeshIslandDecomposition, ConnectedFacesSameIsland)
{
    auto mesh = makeTwoConnectedTriangles();
    ASSERT_TRUE(mesh.isValid());
    auto islands = MeshIslandDecomposition::decompose(mesh);
    EXPECT_EQ(islands.size(), 1u);
}

} // namespace
