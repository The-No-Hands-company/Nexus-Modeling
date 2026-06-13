#include <gtest/gtest.h>

#include <nexus/geometry/MeshAmbientOcclusion.h>
#include <nexus/geometry/Mesh.h>

namespace {

using namespace nexus::geometry;

static Mesh makePlaneWithNormals()
{
    auto plane = primitives::makePlane(10.f, 10.f, 1, 1);
    (void)plane.computeVertexNormals();
    return plane;
}

TEST(MeshAmbientOcclusion, ProducesValuesArrayMatchingVertexCount)
{
    auto plane = makePlaneWithNormals();
    ASSERT_TRUE(plane.isValid());
    ASSERT_TRUE(plane.attributes().hasNormals());
    auto result = MeshAmbientOcclusion::bake(plane);
    EXPECT_EQ(result.ao.size(), plane.attributes().vertexCount());
    EXPECT_EQ(result.bentNormals.size(), plane.attributes().vertexCount());
}

TEST(MeshAmbientOcclusion, ValuesInRange)
{
    auto plane = makePlaneWithNormals();
    ASSERT_TRUE(plane.isValid());
    auto result = MeshAmbientOcclusion::bake(plane);
    ASSERT_FALSE(result.ao.empty());
    for (float v : result.ao) {
        EXPECT_GE(v, 0.f);
        EXPECT_LE(v, 1.f);
    }
}

TEST(MeshAmbientOcclusion, EmptyMeshFails)
{
    Mesh empty;
    auto result = MeshAmbientOcclusion::bake(empty);
    EXPECT_TRUE(result.ao.empty());
    EXPECT_TRUE(result.bentNormals.empty());
}

TEST(MeshAmbientOcclusion, DeterministicSameSeed)
{
    auto plane = makePlaneWithNormals();
    ASSERT_TRUE(plane.isValid());
    AmbientOcclusionOptions opts;
    opts.seed = 99999;
    auto r1 = MeshAmbientOcclusion::bake(plane, opts);
    auto r2 = MeshAmbientOcclusion::bake(plane, opts);
    ASSERT_EQ(r1.ao.size(), r2.ao.size());
    for (size_t i = 0; i < r1.ao.size(); ++i) {
        EXPECT_FLOAT_EQ(r1.ao[i], r2.ao[i]);
    }
}

TEST(MeshAmbientOcclusion, FlatPlaneHasHighAO)
{
    auto plane = makePlaneWithNormals();
    ASSERT_TRUE(plane.isValid());
    AmbientOcclusionOptions opts;
    opts.sampleCount = 16;
    opts.maxDistance = 100.f;
    auto result = MeshAmbientOcclusion::bake(plane, opts);
    ASSERT_FALSE(result.ao.empty());
    for (float v : result.ao) {
        EXPECT_GT(v, 0.9f);
    }
}

TEST(MeshAmbientOcclusion, BentNormalsAreUnitLength)
{
    auto plane = makePlaneWithNormals();
    ASSERT_TRUE(plane.isValid());
    auto result = MeshAmbientOcclusion::bake(plane);
    ASSERT_FALSE(result.bentNormals.empty());
    for (const auto& bn : result.bentNormals) {
        float lenSq = bn.x * bn.x + bn.y * bn.y + bn.z * bn.z;
        EXPECT_NEAR(lenSq, 1.f, 0.01f);
    }
}

TEST(MeshAmbientOcclusion, FlatPlaneBentNormalsNearGeometryNormals)
{
    auto plane = makePlaneWithNormals();
    ASSERT_TRUE(plane.isValid());
    AmbientOcclusionOptions opts;
    opts.sampleCount = 16;
    opts.maxDistance = 100.f;
    auto result = MeshAmbientOcclusion::bake(plane, opts);
    ASSERT_FALSE(result.bentNormals.empty());

    const auto& norms = plane.attributes().normals();
    for (size_t i = 0; i < result.bentNormals.size(); ++i) {
        float d = norms[i].dot(result.bentNormals[i]);
        EXPECT_GT(d, 0.9f);
    }
}

TEST(MeshAmbientOcclusion, SurfaceSamplePathProducesValidOutput)
{
    auto plane = makePlaneWithNormals();
    ASSERT_TRUE(plane.isValid());
    AmbientOcclusionOptions opts;
    opts.sampleCount = 16;
    opts.maxDistance = 100.f;
    opts.surfaceSampleCount = 32;
    auto result = MeshAmbientOcclusion::bake(plane, opts);
    ASSERT_FALSE(result.ao.empty());
    ASSERT_FALSE(result.bentNormals.empty());
    EXPECT_EQ(result.ao.size(), plane.attributes().vertexCount());
    for (float v : result.ao) {
        EXPECT_GE(v, 0.f);
        EXPECT_LE(v, 1.f);
    }
}

TEST(MeshAmbientOcclusion, NonBVHPathProducesValidOutput)
{
    auto plane = makePlaneWithNormals();
    ASSERT_TRUE(plane.isValid());
    AmbientOcclusionOptions opts;
    opts.sampleCount = 8;
    opts.maxDistance = 100.f;
    opts.useBVH = false;
    auto result = MeshAmbientOcclusion::bake(plane, opts);
    ASSERT_FALSE(result.ao.empty());
    ASSERT_FALSE(result.bentNormals.empty());
    EXPECT_EQ(result.ao.size(), plane.attributes().vertexCount());
    for (float v : result.ao) {
        EXPECT_GE(v, 0.f);
        EXPECT_LE(v, 1.f);
    }
}

TEST(MeshAmbientOcclusion, BVHAndNonBVHProduceSimilarResults)
{
    auto box = primitives::makeBox(1.f, 1.f, 1.f);
    (void)box.computeVertexNormals();
    ASSERT_TRUE(box.isValid());
    ASSERT_TRUE(box.attributes().hasNormals());

    AmbientOcclusionOptions opts;
    opts.sampleCount = 32;
    opts.maxDistance = 10.f;
    opts.seed = 42;
    opts.useBVH = true;
    auto bvhResult = MeshAmbientOcclusion::bake(box, opts);

    opts.useBVH = false;
    auto bruteResult = MeshAmbientOcclusion::bake(box, opts);

    ASSERT_EQ(bvhResult.ao.size(), bruteResult.ao.size());
    ASSERT_EQ(bvhResult.bentNormals.size(), bruteResult.bentNormals.size());

    for (size_t i = 0; i < bvhResult.ao.size(); ++i) {
        EXPECT_NEAR(bvhResult.ao[i], bruteResult.ao[i], 0.001f);
        EXPECT_NEAR(bvhResult.bentNormals[i].x, bruteResult.bentNormals[i].x, 0.001f);
        EXPECT_NEAR(bvhResult.bentNormals[i].y, bruteResult.bentNormals[i].y, 0.001f);
        EXPECT_NEAR(bvhResult.bentNormals[i].z, bruteResult.bentNormals[i].z, 0.001f);
    }
}

} // namespace
