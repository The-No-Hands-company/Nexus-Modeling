#include <gtest/gtest.h>

#include <nexus/geometry/MeshNormals.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static Mesh makeXYQuad()
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    Face f;
    f.indices = {0, 1, 2, 3};
    mesh.topology().addFace(f);
    return mesh;
}

TEST(MeshNormals, SmoothProducesNormalsForXYQuad)
{
    Mesh mesh = makeXYQuad();
    ASSERT_TRUE(mesh.isValid());

    NormalOptions opts;
    opts.mode = NormalMode::Smooth;

    EXPECT_TRUE(MeshNormals::compute(mesh, opts));
    EXPECT_TRUE(mesh.attributes().hasNormals());

    const auto& norms = mesh.attributes().normals();
    for (const auto& n : norms) {
        EXPECT_NEAR(n.length(), 1.f, 1e-5f);
        EXPECT_NEAR(n.z, 1.f, 1e-4f);
    }
}

TEST(MeshNormals, FlatModeIncreasesVertexCount)
{
    Mesh mesh = makeXYQuad();
    ASSERT_TRUE(mesh.isValid());
    size_t originalVertexCount = mesh.attributes().vertexCount();

    NormalOptions opts;
    opts.mode = NormalMode::Flat;

    EXPECT_TRUE(MeshNormals::compute(mesh, opts));

    size_t newVertexCount = mesh.attributes().vertexCount();
    EXPECT_GT(newVertexCount, originalVertexCount);
}

TEST(MeshNormals, SmoothIsDefaultMode)
{
    Mesh mesh = makeXYQuad();
    ASSERT_TRUE(mesh.isValid());

    NormalOptions opts;
    EXPECT_EQ(opts.mode, NormalMode::Smooth);
}

} // namespace
