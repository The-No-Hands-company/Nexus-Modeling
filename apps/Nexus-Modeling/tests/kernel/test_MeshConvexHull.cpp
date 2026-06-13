#include <gtest/gtest.h>

#include <nexus/geometry/MeshConvexHull.h>
#include <nexus/geometry/Mesh.h>

namespace {

using namespace nexus::geometry;
using nexus::render::Vec3;

TEST(MeshConvexHull, TetrahedronFromFourPoints)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 0.f, 1.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_EQ(hull.vertices.size(), 4u);
    EXPECT_GE(hull.faces.size(), 4u);
}

TEST(MeshConvexHull, CubeFromEightPoints)
{
    std::vector<Vec3> points = {
        {-1.f, -1.f, -1.f},
        { 1.f, -1.f, -1.f},
        { 1.f,  1.f, -1.f},
        {-1.f,  1.f, -1.f},
        {-1.f, -1.f,  1.f},
        { 1.f, -1.f,  1.f},
        { 1.f,  1.f,  1.f},
        {-1.f,  1.f,  1.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_GE(hull.vertices.size(), 8u);
    EXPECT_GE(hull.faces.size(), 6u);
}

TEST(MeshConvexHull, FromMeshConveniencePasses)
{
    auto box = primitives::makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    auto hull = MeshConvexHull::fromMesh(box);
    EXPECT_GE(hull.vertices.size(), 8u);
    EXPECT_GE(hull.faces.size(), 4u);
}

TEST(MeshConvexHull, TooFewPointsFails)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_TRUE(hull.vertices.empty());
    EXPECT_TRUE(hull.faces.empty());
}

TEST(MeshConvexHull, CoplanarPointsFails)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {1.f, 1.f, 0.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_TRUE(hull.faces.empty());
}

TEST(MeshConvexHull, SinglePointFails)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_TRUE(hull.vertices.empty());
    EXPECT_TRUE(hull.faces.empty());
}

TEST(MeshConvexHull, CollinearPointsFail)
{
    std::vector<Vec3> points = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {2.f, 0.f, 0.f},
    };
    auto hull = MeshConvexHull::build(points);
    EXPECT_TRUE(hull.faces.empty());
}

} // namespace
