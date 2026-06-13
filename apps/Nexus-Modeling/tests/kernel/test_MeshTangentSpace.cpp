#include <gtest/gtest.h>

#include <nexus/geometry/MeshTangentSpace.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>

namespace {

using namespace nexus::geometry;

static Mesh makeMeshWithNormalsAndUVs()
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.attributes().setNormals({
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    Face f;
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);
    return mesh;
}

TEST(MeshTangentSpace, ProducesTangents)
{
    auto mesh = makeMeshWithNormalsAndUVs();
    ASSERT_TRUE(mesh.isValid());
    ASSERT_TRUE(mesh.attributes().hasNormals());
    ASSERT_TRUE(mesh.attributes().hasUVs());

    Mesh result = MeshTangentSpace::compute(mesh);
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasTangents());
    EXPECT_EQ(result.attributes().tangents().size(), mesh.attributes().vertexCount());
}

TEST(MeshTangentSpace, MissingNormalsReturnsNoTangents)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    Face f;
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);
    ASSERT_TRUE(mesh.isValid());

    Mesh result = MeshTangentSpace::compute(mesh);
    EXPECT_FALSE(result.attributes().hasTangents());
}

TEST(MeshTangentSpace, MissingUVsReturnsNoTangents)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.attributes().setNormals({
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
    });
    Face f;
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);
    ASSERT_TRUE(mesh.isValid());

    Mesh result = MeshTangentSpace::compute(mesh);
    EXPECT_FALSE(result.attributes().hasTangents());
}

TEST(MeshTangentSpace, TangentHandednessIsPlusMinusOne)
{
    auto mesh = makeMeshWithNormalsAndUVs();
    ASSERT_TRUE(mesh.isValid());

    Mesh result = MeshTangentSpace::compute(mesh);
    ASSERT_TRUE(result.attributes().hasTangents());

    for (const auto& t : result.attributes().tangents()) {
        EXPECT_TRUE(std::fabs(t.w - 1.f) < 1e-4f || std::fabs(t.w + 1.f) < 1e-4f);
    }
}

} // namespace
