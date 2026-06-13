#include <gtest/gtest.h>

#include <nexus/geometry/MeshLaplacian.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static Mesh makeSmallPlane()
{
    auto plane = primitives::makePlane(10.f, 10.f, 2, 2);
    (void)plane.topology().triangulate();
    return plane;
}

TEST(MeshLaplacian, SmoothPreservesVertexCount)
{
    auto plane = makeSmallPlane();
    ASSERT_TRUE(plane.isValid());
    size_t origCount = plane.attributes().vertexCount();
    SmoothOptions opts;
    opts.iterations = 3;
    opts.lambda = 0.3f;
    Mesh smoothed = MeshLaplacian::smooth(plane, opts);
    ASSERT_TRUE(smoothed.isValid());
    EXPECT_EQ(smoothed.attributes().vertexCount(), origCount);
}

TEST(MeshLaplacian, PlanarMeshStaysPlanarAfterSmoothing)
{
    auto plane = makeSmallPlane();
    ASSERT_TRUE(plane.isValid());
    SmoothOptions opts;
    opts.iterations = 5;
    opts.lambda = 0.5f;
    Mesh smoothed = MeshLaplacian::smooth(plane, opts);
    ASSERT_TRUE(smoothed.isValid());
    for (const auto& p : smoothed.attributes().positions()) {
        EXPECT_NEAR(p.y, 0.f, 1e-4f);
    }
}

TEST(MeshLaplacian, MeanCurvatureReturnsArrayOfCorrectSize)
{
    auto plane = makeSmallPlane();
    ASSERT_TRUE(plane.isValid());
    auto mc = MeshLaplacian::meanCurvature(plane);
    EXPECT_EQ(mc.size(), plane.attributes().vertexCount());
}

TEST(MeshLaplacian, MeanCurvatureNonNegative)
{
    auto plane = makeSmallPlane();
    ASSERT_TRUE(plane.isValid());
    auto mc = MeshLaplacian::meanCurvature(plane);
    ASSERT_FALSE(mc.empty());
    for (float v : mc) {
        EXPECT_GE(v, 0.f);
    }
}

} // namespace
