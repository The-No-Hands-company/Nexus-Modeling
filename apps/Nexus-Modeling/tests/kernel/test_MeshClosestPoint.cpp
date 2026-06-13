#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshClosestPoint.h>

#include <cmath>
#include <limits>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshClosestPoint, PointOnSurfaceReturnsSamePosition)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());

    nexus::render::Vec3 query{0.f, 0.f, 0.f};
    MeshClosestHit hit = MeshClosestPoint::closest(plane, query);

    EXPECT_NEAR(hit.distanceSquared, 0.f, 1e-4f);
    EXPECT_NEAR(hit.point.x, 0.f, 1e-4f);
    EXPECT_NEAR(hit.point.y, 0.f, 1e-4f);
    EXPECT_NEAR(hit.point.z, 0.f, 1e-4f);
}

TEST(MeshClosestPoint, PointAboveSurfaceReturnsCorrectProjection)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());

    nexus::render::Vec3 query{0.f, 2.f, 0.f};
    MeshClosestHit hit = MeshClosestPoint::closest(plane, query);

    EXPECT_NEAR(hit.distanceSquared, 4.f, 1e-3f);
    EXPECT_NEAR(hit.point.x, 0.f, 1e-4f);
    EXPECT_NEAR(hit.point.y, 0.f, 1e-4f);
    EXPECT_NEAR(hit.point.z, 0.f, 1e-4f);
}

TEST(MeshClosestPoint, BatchQueryReturnsCorrectCount)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());

    std::vector<nexus::render::Vec3> queries = {
        {0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.5f, -2.f, 0.f},
    };

    MeshClosestBatch batch = MeshClosestPoint::query(plane, queries);
    EXPECT_EQ(batch.hits.size(), 3u);

    for (const auto& hit : batch.hits) {
        EXPECT_LT(hit.distanceSquared, std::numeric_limits<float>::max());
    }
}

TEST(MeshClosestPoint, EmptyMeshFails)
{
    Mesh empty;

    MeshClosestHit hit = MeshClosestPoint::closest(empty, {0.f, 0.f, 0.f});
    EXPECT_EQ(hit.distanceSquared, std::numeric_limits<float>::max());
    EXPECT_EQ(hit.triangle, 0u);
    EXPECT_FLOAT_EQ(hit.baryU, 0.f);
    EXPECT_FLOAT_EQ(hit.baryV, 0.f);
}

TEST(MeshClosestPoint, TriangleIndexValid)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());

    nexus::render::Vec3 query{0.f, 1.f, 0.f};
    MeshClosestHit hit = MeshClosestPoint::closest(plane, query);

    EXPECT_EQ(hit.triangle, 0u);
    EXPECT_GE(hit.baryU, 0.f);
    EXPECT_GE(hit.baryV, 0.f);
    EXPECT_LE(hit.baryU + hit.baryV, 1.f + 1e-4f);
}

TEST(MeshClosestPoint, EmptyQueriesReturnsOk)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());

    MeshClosestBatch batch = MeshClosestPoint::query(plane, {});
    EXPECT_EQ(batch.hits.size(), 0u);
}
