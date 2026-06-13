#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshGeodesicDistance.h>

#include <cmath>
#include <limits>

using namespace nexus::geometry;

namespace {

Mesh makeSingleTriangle()
{
    std::vector<nexus::render::Vec3> positions = {
        { 0.f, 0.f,  0.f},
        { 1.f, 0.f,  0.f},
        { 0.f, 0.f,  1.f},
    };
    Face f{};
    f.indices = {0, 1, 2};
    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.topology().addFace(std::move(f));
    return mesh;
}

Mesh makeDisconnectedMesh()
{
    std::vector<nexus::render::Vec3> positions = {
        { 0.f, 0.f, 0.f}, { 1.f, 0.f, 0.f}, { 0.f, 0.f, 1.f},
        { 5.f, 0.f, 0.f}, { 6.f, 0.f, 0.f}, { 5.f, 0.f, 1.f},
    };
    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    {
        Face f{};
        f.indices = {0, 1, 2};
        mesh.topology().addFace(std::move(f));
    }
    {
        Face f2{};
        f2.indices = {3, 4, 5};
        mesh.topology().addFace(std::move(f2));
    }
    return mesh;
}

} // namespace

TEST(MeshGeodesicDistance, SourceToSelfIsZero)
{
    Mesh mesh = makeSingleTriangle();
    ASSERT_TRUE(mesh.isValid());

    GeodesicResult result = MeshGeodesicDistance::fromVertex(mesh, 0);

    ASSERT_EQ(result.distances.size(), mesh.attributes().vertexCount());
    EXPECT_FLOAT_EQ(result.distances[0], 0.f);
}

TEST(MeshGeodesicDistance, DistanceToNeighborIsEdgeLength)
{
    Mesh mesh = makeSingleTriangle();
    ASSERT_TRUE(mesh.isValid());

    GeodesicResult result = MeshGeodesicDistance::fromVertex(mesh, 0);

    ASSERT_GE(result.distances.size(), 3u);
    EXPECT_NEAR(result.distances[1], 1.f, 1e-4f);
    EXPECT_NEAR(result.distances[2], 1.f, 1e-4f);
}

TEST(MeshGeodesicDistance, MultiSourceBothDistancesZero)
{
    Mesh mesh = makeSingleTriangle();
    ASSERT_TRUE(mesh.isValid());

    GeodesicResult result = MeshGeodesicDistance::fromVertices(mesh, {0, 1});

    ASSERT_GE(result.distances.size(), 3u);
    EXPECT_FLOAT_EQ(result.distances[0], 0.f);
    EXPECT_FLOAT_EQ(result.distances[1], 0.f);
    EXPECT_TRUE(std::isfinite(result.distances[2]));
}

TEST(MeshGeodesicDistance, UnreachableIsInfinity)
{
    Mesh mesh = makeDisconnectedMesh();
    ASSERT_TRUE(mesh.isValid());

    GeodesicResult result = MeshGeodesicDistance::fromVertex(mesh, 0);

    ASSERT_EQ(result.distances.size(), 6u);
    EXPECT_GE(result.distances[3], std::numeric_limits<float>::max() * 0.5f);
    EXPECT_GE(result.distances[4], std::numeric_limits<float>::max() * 0.5f);
    EXPECT_GE(result.distances[5], std::numeric_limits<float>::max() * 0.5f);
}

TEST(MeshGeodesicDistance, EmptyMeshReturnsEmpty)
{
    Mesh empty;

    GeodesicResult result = MeshGeodesicDistance::fromVertex(empty, 0);

    EXPECT_TRUE(result.distances.empty());
}
