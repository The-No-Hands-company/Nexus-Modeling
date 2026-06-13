#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshThicken.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshThicken, ProducesMoreFacesThanInput)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());
    const size_t originalFaces = plane.topology().faceCount();

    ThickenOptions opts;
    opts.thickness = 0.5f;
    Mesh result = MeshThicken::solidify(plane, opts);

    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), originalFaces);
}

TEST(MeshThicken, OffsetAppliedCorrectly)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());
    const auto& origPos = plane.attributes().positions();
    const size_t vc = origPos.size();

    ThickenOptions opts;
    opts.thickness = 0.5f;
    Mesh result = MeshThicken::solidify(plane, opts);

    ASSERT_EQ(result.attributes().vertexCount(), vc * 2);

    const auto& newPos = result.attributes().positions();
    for (size_t i = 0; i < vc; ++i) {
        EXPECT_FLOAT_EQ(newPos[i].x, origPos[i].x);
        EXPECT_FLOAT_EQ(newPos[i].y, origPos[i].y);
        EXPECT_FLOAT_EQ(newPos[i].z, origPos[i].z);
    }

    for (size_t i = 0; i < vc; ++i) {
        float dx = newPos[vc + i].x - origPos[i].x;
        float dy = newPos[vc + i].y - origPos[i].y;
        float dz = newPos[vc + i].z - origPos[i].z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        EXPECT_NEAR(dist, opts.thickness, 1e-4f);
    }
}

TEST(MeshThicken, RecomputeNormalsWorks)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());

    ThickenOptions opts;
    opts.thickness = 0.3f;
    Mesh result = MeshThicken::solidify(plane, opts);

    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasNormals());

    for (const auto& n : result.attributes().normals()) {
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        EXPECT_NEAR(len, 1.f, 1e-4f);
    }
}

TEST(MeshThicken, EmptyMeshFails)
{
    Mesh empty;

    ThickenOptions opts;
    opts.thickness = 0.5f;
    Mesh result = MeshThicken::solidify(empty, opts);

    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.topology().faceCount(), 0u);
    EXPECT_EQ(result.attributes().vertexCount(), 0u);
}

TEST(MeshThicken, ZeroThicknessFails)
{
    Mesh plane = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(plane.isValid());

    ThickenOptions opts;
    opts.thickness = 0.f;
    Mesh result = MeshThicken::solidify(plane, opts);

    EXPECT_FALSE(result.isValid());
}
