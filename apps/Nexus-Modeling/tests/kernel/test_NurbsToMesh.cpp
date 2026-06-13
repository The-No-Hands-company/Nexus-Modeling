#include <gtest/gtest.h>

#include <nexus/geometry/NurbsToMesh.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cmath>
#include <vector>

using namespace nexus::geometry;

static NurbsSurface makePlanarSurface() {
    std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {2.f, 0.f, 0.f}, {3.f, 0.f, 0.f},
        {0.f, 1.f, 0.f}, {1.f, 1.f, 0.f}, {2.f, 1.f, 0.f}, {3.f, 1.f, 0.f},
        {0.f, 2.f, 0.f}, {1.f, 2.f, 0.f}, {2.f, 2.f, 0.f}, {3.f, 2.f, 0.f},
        {0.f, 3.f, 0.f}, {1.f, 3.f, 0.f}, {2.f, 3.f, 0.f}, {3.f, 3.f, 0.f},
    };
    return NurbsSurface(3, 3, kU, kV, ctl, 4, 4);
}

static NurbsSurface makeBiquadraticMound() {
    std::vector<float> kU = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 1.f}, {2.f, 0.f, 0.f},
        {0.f, 1.f, 1.f}, {1.f, 1.f, 2.f}, {2.f, 1.f, 1.f},
        {0.f, 2.f, 0.f}, {1.f, 2.f, 1.f}, {2.f, 2.f, 0.f},
    };
    return NurbsSurface(2, 2, kU, kV, ctl, 3, 3);
}

TEST(NurbsToMesh, InvalidSurfaceReturnsEmpty) {
    NurbsSurface bad;
    auto result = tessellateNurbs(bad);
    EXPECT_EQ(result.totalTriangles, 0);
    EXPECT_FALSE(result.mesh.isValid());
}

TEST(NurbsToMesh, PlanarSurfaceProducesValidMesh) {
    auto surface = makePlanarSurface();
    TessellationOptions opts;
    opts.initialResU = 4;
    opts.initialResV = 4;
    opts.maxDepth = 0;
    opts.tolerance = 0.01f;

    auto result = tessellateNurbs(surface, opts);
    EXPECT_GT(result.totalTriangles, 0);
    EXPECT_TRUE(result.mesh.isValid());
    EXPECT_TRUE(result.mesh.attributes().hasPositions());
    EXPECT_TRUE(result.mesh.attributes().hasNormals());
    EXPECT_TRUE(result.mesh.attributes().hasUVs());
}

TEST(NurbsToMesh, PlanarSurfaceNormalsAreUnitLength) {
    auto surface = makePlanarSurface();
    TessellationOptions opts;
    opts.initialResU = 4;
    opts.initialResV = 4;
    opts.maxDepth = 0;

    auto result = tessellateNurbs(surface, opts);
    const auto& norms = result.mesh.attributes().normals();
    for (const auto& n : norms) {
        EXPECT_NEAR(n.length(), 1.f, 1e-4f);
        EXPECT_NEAR(n.x, 0.f, 1e-4f);
        EXPECT_NEAR(n.y, 0.f, 1e-4f);
        EXPECT_NEAR(std::abs(n.z), 1.f, 1e-4f);
    }
    float zSign = norms.front().z;
    for (const auto& n : norms)
        EXPECT_EQ(std::signbit(n.z), std::signbit(zSign));
}

TEST(NurbsToMesh, BiquadraticMoundSubdivides) {
    auto surface = makeBiquadraticMound();
    TessellationOptions opts;
    opts.initialResU = 2;
    opts.initialResV = 2;
    opts.maxDepth = 3;
    opts.tolerance = 0.01f;

    auto result = tessellateNurbs(surface, opts);
    EXPECT_GT(result.totalTriangles, 0);
    EXPECT_TRUE(result.mesh.isValid());
}

TEST(NurbsToMesh, TightToleranceIncreasesTriangleCount) {
    auto surface = makeBiquadraticMound();
    TessellationOptions looseOpts;
    looseOpts.initialResU = 2;
    looseOpts.initialResV = 2;
    looseOpts.maxDepth = 3;
    looseOpts.tolerance = 10.f;

    TessellationOptions tightOpts = looseOpts;
    tightOpts.tolerance = 0.001f;

    auto resultLoose  = tessellateNurbs(surface, looseOpts);
    auto resultTight  = tessellateNurbs(surface, tightOpts);
    EXPECT_GE(resultTight.totalTriangles, resultLoose.totalTriangles);
}

TEST(NurbsToMesh, InitialResolutionScalesTriangleCount) {
    auto surface = makeBiquadraticMound();
    TessellationOptions lowResOpts;
    lowResOpts.initialResU = 1;
    lowResOpts.initialResV = 1;
    lowResOpts.maxDepth = 0;

    TessellationOptions highResOpts;
    highResOpts.initialResU = 4;
    highResOpts.initialResV = 4;
    highResOpts.maxDepth = 0;

    auto resultLow  = tessellateNurbs(surface, lowResOpts);
    auto resultHigh = tessellateNurbs(surface, highResOpts);
    EXPECT_GT(resultHigh.totalTriangles, resultLow.totalTriangles);
}

TEST(NurbsToMesh, UVsInParameterDomain) {
    auto surface = makePlanarSurface();
    TessellationOptions opts;
    opts.initialResU = 2;
    opts.initialResV = 2;
    opts.maxDepth = 0;

    auto result = tessellateNurbs(surface, opts);
    const auto& uvs = result.mesh.attributes().uvs();

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();

    for (const auto& uv : uvs) {
        EXPECT_GE(uv.u, uMin);
        EXPECT_LE(uv.u, uMax);
        EXPECT_GE(uv.v, vMin);
        EXPECT_LE(uv.v, vMax);
    }
}

TEST(NurbsToMesh, MaxDepthRespected) {
    auto surface = makeBiquadraticMound();
    TessellationOptions opts;
    opts.initialResU = 1;
    opts.initialResV = 1;
    opts.maxDepth = 2;
    opts.tolerance = 0.f;

    auto result = tessellateNurbs(surface, opts);
    EXPECT_LE(result.maxDepthReached, opts.maxDepth);
    EXPECT_GT(result.totalTriangles, 0);
    EXPECT_TRUE(result.mesh.isValid());
}
