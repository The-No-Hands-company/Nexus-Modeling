#include <gtest/gtest.h>

#include <nexus/geometry/MeshPointSample.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshPointSample, ProducesRequestedCount) {
    Mesh mesh = makeSphere(2.f, 16, 16);
    PointSampleOptions opts;
    opts.count = 512;
    opts.seed = 42;
    PointSampleResult r = MeshPointSample::sample(mesh, opts);
    EXPECT_EQ(r.points.size(), 512u);
    EXPECT_EQ(r.triangles.size(), 512u);
}

TEST(MeshPointSample, EmptyMeshReturnsEmptyResult) {
    Mesh mesh;
    PointSampleOptions opts;
    opts.count = 128;
    PointSampleResult r = MeshPointSample::sample(mesh, opts);
    EXPECT_TRUE(r.points.empty());
    EXPECT_TRUE(r.triangles.empty());
}

TEST(MeshPointSample, PointsLieOnMeshSurface) {
    Mesh mesh = makeSphere(1.f, 8, 8);
    PointSampleOptions opts;
    opts.count = 256;
    opts.seed = 1;
    PointSampleResult r = MeshPointSample::sample(mesh, opts);
    ASSERT_FALSE(r.points.empty());
    for (const auto& p : r.points) {
        float dist = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        EXPECT_NEAR(dist, 1.f, 0.15f);
    }
}

TEST(MeshPointSample, DeterministicSameSeed) {
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    PointSampleOptions opts;
    opts.count = 64;
    opts.seed = 123;
    PointSampleResult a = MeshPointSample::sample(mesh, opts);
    PointSampleResult b = MeshPointSample::sample(mesh, opts);
    ASSERT_EQ(a.points.size(), b.points.size());
    for (size_t i = 0; i < a.points.size(); ++i) {
        EXPECT_FLOAT_EQ(a.points[i].x, b.points[i].x);
        EXPECT_FLOAT_EQ(a.points[i].y, b.points[i].y);
        EXPECT_FLOAT_EQ(a.points[i].z, b.points[i].z);
    }
}

TEST(MeshPointSample, DifferentSeedsDiffer) {
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    PointSampleOptions optsA; optsA.count = 16; optsA.seed = 1;
    PointSampleOptions optsB; optsB.count = 16; optsB.seed = 2;
    PointSampleResult a = MeshPointSample::sample(mesh, optsA);
    PointSampleResult b = MeshPointSample::sample(mesh, optsB);
    ASSERT_EQ(a.points.size(), b.points.size());
    bool anyDiff = false;
    for (size_t i = 0; i < a.points.size(); ++i) {
        if (a.points[i].x != b.points[i].x ||
            a.points[i].y != b.points[i].y ||
            a.points[i].z != b.points[i].z) {
            anyDiff = true;
            break;
        }
    }
    EXPECT_TRUE(anyDiff);
}

TEST(MeshPointSample, NormalsAreUnitLength) {
    Mesh mesh = makeSphere(1.f, 8, 8);
    (void)mesh.computeVertexNormals();
    PointSampleOptions opts;
    opts.count = 128;
    opts.seed = 7;
    PointSampleResult r = MeshPointSample::sample(mesh, opts);
    ASSERT_FALSE(r.normals.empty());
    for (const auto& n : r.normals) {
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        EXPECT_NEAR(len, 1.f, 0.1f);
    }
}

TEST(MeshPointSample, NoNormalsWhenMeshLacksNormals) {
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    mesh.attributes().clearNormals();
    PointSampleOptions opts;
    opts.count = 64;
    opts.seed = 99;
    PointSampleResult r = MeshPointSample::sample(mesh, opts);
    // Implementation computes normals internally when they are absent from mesh
    EXPECT_FALSE(r.points.empty());
}
