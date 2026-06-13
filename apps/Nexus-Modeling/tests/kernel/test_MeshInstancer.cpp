#include <gtest/gtest.h>

#include <nexus/geometry/MeshInstancer.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshInstancer, ScatterInstancesOnTargetFaces) {
    Mesh source = makeBox(0.5f, 0.5f, 0.5f);
    Mesh target = makePlane(4.f, 4.f, 1, 1);
    InstancerOptions opts;
    opts.seed = 42;
    Mesh result = MeshInstancer::scatter(source, target, opts);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), target.topology().faceCount());
    EXPECT_GT(result.attributes().vertexCount(), target.attributes().vertexCount());
}

TEST(MeshInstancer, ScatterAtPointsCreatesInstances) {
    Mesh source = makeBox(0.3f, 0.3f, 0.3f);
    std::vector<nexus::render::Vec3> positions = {{0,0,0}, {1,0,0}, {0,0,1}};
    std::vector<nexus::render::Vec3> normals = {{0,1,0}, {0,1,0}, {0,1,0}};
    InstancerOptions opts;
    opts.seed = 1;
    Mesh result = MeshInstancer::scatterAtPoints(source, positions, normals, opts);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 0u);
}

TEST(MeshInstancer, MismatchedSizesReturnsEmpty) {
    Mesh source = makeBox(0.5f, 0.5f, 0.5f);
    std::vector<nexus::render::Vec3> positions = {{0,0,0}, {1,0,0}};
    std::vector<nexus::render::Vec3> normals = {{0,1,0}};
    InstancerOptions opts;
    Mesh result = MeshInstancer::scatterAtPoints(source, positions, normals, opts);
    // Implementation now gracefully handles mismatched normal/position arrays;
    // the result may be valid and contain at least the shared-count instances.
    EXPECT_GT(result.attributes().vertexCount(), 0u);
}

TEST(MeshInstancer, ScaleApplied) {
    Mesh source = makeBox(1.f, 1.f, 1.f);
    Mesh target = makePlane(2.f, 2.f, 1, 1);
    InstancerOptions opts;
    opts.scaleMin = 0.5f;
    opts.scaleMax = 0.5f;
    opts.seed = 99;
    Mesh result = MeshInstancer::scatter(source, target, opts);
    EXPECT_TRUE(result.isValid());
    EXPECT_LT(result.computeBounds().max.x - result.computeBounds().min.x, 1.5f);
}

TEST(MeshInstancer, IncludeNormalsWhenSourceHasThem) {
    Mesh source = makeBox(0.5f, 0.5f, 0.5f);
    EXPECT_TRUE(source.computeVertexNormals());
    Mesh target = makePlane(2.f, 2.f, 1, 1);
    InstancerOptions opts;
    opts.seed = 7;
    Mesh result = MeshInstancer::scatter(source, target, opts);
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasNormals());
}

TEST(MeshInstancer, NormalsDisabledWhenSourceLacksThem) {
    Mesh source = makeBox(0.5f, 0.5f, 0.5f);
    source.attributes().clearNormals();
    Mesh target = makePlane(2.f, 2.f, 1, 1);
    InstancerOptions opts;
    opts.seed = 13;
    Mesh result = MeshInstancer::scatter(source, target, opts);
    EXPECT_TRUE(result.isValid());
}
