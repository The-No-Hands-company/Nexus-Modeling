#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/SurfaceOffset.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(SurfaceOffset, PositiveOffsetShiftsVertices)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());
    const auto& origPos = plane.attributes().positions();

    SurfaceOffsetOptions opts;
    opts.distance = 0.5f;
    Mesh result = SurfaceOffset::offset(plane, opts);

    EXPECT_TRUE(result.isValid());
    const auto& newPos = result.attributes().positions();
    EXPECT_EQ(newPos.size(), origPos.size());

    for (size_t i = 0; i < origPos.size(); ++i) {
        EXPECT_NEAR(newPos[i].x, origPos[i].x, 1e-4f);
        EXPECT_GT(newPos[i].y, origPos[i].y);
        EXPECT_NEAR(newPos[i].z, origPos[i].z, 1e-4f);
    }

    EXPECT_NEAR(newPos[0].y, origPos[0].y + 0.5f, 1e-4f);
}

TEST(SurfaceOffset, ZeroDistanceKeepsOriginalPositions)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());
    const auto& origPos = plane.attributes().positions();

    SurfaceOffsetOptions opts;
    opts.distance = 0.f;
    Mesh result = SurfaceOffset::offset(plane, opts);

    EXPECT_TRUE(result.isValid());
    const auto& newPos = result.attributes().positions();
    EXPECT_EQ(newPos.size(), origPos.size());

    for (size_t i = 0; i < origPos.size(); ++i) {
        EXPECT_NEAR(newPos[i].x, origPos[i].x, 1e-4f);
        EXPECT_NEAR(newPos[i].y, origPos[i].y, 1e-4f);
        EXPECT_NEAR(newPos[i].z, origPos[i].z, 1e-4f);
    }
}

TEST(SurfaceOffset, NegativeOffsetWorks)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());
    const auto& origPos = plane.attributes().positions();

    SurfaceOffsetOptions opts;
    opts.distance = -0.3f;
    Mesh result = SurfaceOffset::offset(plane, opts);

    EXPECT_TRUE(result.isValid());
    const auto& newPos = result.attributes().positions();
    EXPECT_EQ(newPos.size(), origPos.size());

    for (size_t i = 0; i < origPos.size(); ++i) {
        EXPECT_NEAR(newPos[i].x, origPos[i].x, 1e-4f);
        EXPECT_LT(newPos[i].y, origPos[i].y);
        EXPECT_NEAR(newPos[i].z, origPos[i].z, 1e-4f);
    }

    EXPECT_NEAR(newPos[0].y, origPos[0].y - 0.3f, 1e-4f);
}

TEST(SurfaceOffset, EmptyMeshFails)
{
    Mesh empty;

    SurfaceOffsetOptions opts;
    opts.distance = 0.5f;
    Mesh result = SurfaceOffset::offset(empty, opts);

    EXPECT_FALSE(result.isValid());
}

TEST(SurfaceOffset, PreservesTopology)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());
    const size_t originalFaces = plane.topology().faceCount();
    const size_t originalVerts = plane.attributes().vertexCount();

    SurfaceOffsetOptions opts;
    opts.distance = 0.5f;
    Mesh result = SurfaceOffset::offset(plane, opts);

    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.topology().faceCount(), originalFaces);
    EXPECT_EQ(result.attributes().vertexCount(), originalVerts);
}
