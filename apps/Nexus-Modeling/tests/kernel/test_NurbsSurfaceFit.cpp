#include <gtest/gtest.h>

#include <nexus/geometry/NurbsSurfaceFit.h>

#include <cmath>
#include <vector>

using namespace nexus::geometry;

static std::vector<Vec3> makePlanarGrid(int32_t nu, int32_t nv) {
    std::vector<Vec3> grid;
    grid.reserve(static_cast<size_t>(nu * nv));
    for (int32_t i = 0; i < nu; ++i) {
        for (int32_t j = 0; j < nv; ++j) {
            float x = (nu > 1) ? static_cast<float>(i) / static_cast<float>(nu - 1) : 0.f;
            float y = (nv > 1) ? static_cast<float>(j) / static_cast<float>(nv - 1) : 0.f;
            grid.push_back({x, y, 0.f});
        }
    }
    return grid;
}

TEST(NurbsSurfaceFit, FitPlanarGridProducesValidSurface) {
    int32_t nu = 4;
    int32_t nv = 4;
    auto grid = makePlanarGrid(nu, nv);

    NurbsSurfaceFitOptions opts;
    NurbsSurface surface = NurbsSurfaceFit::fit(grid, nu, nv, opts);
    EXPECT_TRUE(surface.isValid());
    EXPECT_EQ(surface.controlPointCountU(), nu);
    EXPECT_EQ(surface.controlPointCountV(), nv);

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();
    float uMid = (uMin + uMax) * 0.5f;
    float vMid = (vMin + vMax) * 0.5f;
    Vec3 p = surface.evaluate(uMid, vMid);
    EXPECT_NEAR(p.z, 0.f, 0.001f);
}

TEST(NurbsSurfaceFit, DimensionMismatchFails) {
    int32_t nu = 4;
    int32_t nv = 4;
    auto grid = makePlanarGrid(nu, nv);

    NurbsSurface surface = NurbsSurfaceFit::fit(grid, 5, nv);
    EXPECT_FALSE(surface.isValid());
}

TEST(NurbsSurfaceFit, TooFewGridPointsFails) {
    auto grid = makePlanarGrid(2, 2);

    NurbsSurface surface = NurbsSurfaceFit::fit(grid, 1, 2);
    EXPECT_FALSE(surface.isValid());
}

TEST(NurbsSurfaceFit, UniformParamsInterpolation) {
    int32_t nu = 5;
    int32_t nv = 5;
    auto grid = makePlanarGrid(nu, nv);

    NurbsSurfaceFitOptions opts;
    opts.uniformParams = true;
    NurbsSurface surface = NurbsSurfaceFit::fit(grid, nu, nv, opts);
    EXPECT_TRUE(surface.isValid());
    EXPECT_EQ(surface.controlPointCountU(), nu);
    EXPECT_EQ(surface.controlPointCountV(), nv);

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();
    float uMid = (uMin + uMax) * 0.5f;
    float vMid = (vMin + vMax) * 0.5f;
    Vec3 p = surface.evaluate(uMid, vMid);
    EXPECT_NEAR(p.z, 0.f, 0.001f);
}

TEST(NurbsSurfaceFit, ApproximationProducesValidSurface) {
    int32_t nu = 8;
    int32_t nv = 8;
    auto grid = makePlanarGrid(nu, nv);

    NurbsSurfaceFitOptions opts;
    opts.useApproximation = true;
    NurbsSurface surface = NurbsSurfaceFit::fit(grid, nu, nv, opts);
    EXPECT_TRUE(surface.isValid());
    EXPECT_EQ(surface.controlPointCountU(), nu / 2);
    EXPECT_EQ(surface.controlPointCountV(), nv / 2);

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();
    float uMid = (uMin + uMax) * 0.5f;
    float vMid = (vMin + vMax) * 0.5f;
    Vec3 p = surface.evaluate(uMid, vMid);
    EXPECT_NEAR(p.z, 0.f, 0.001f);
}

TEST(NurbsSurfaceFit, ApproximationWithCustomControlPoints) {
    int32_t nu = 12;
    int32_t nv = 10;
    auto grid = makePlanarGrid(nu, nv);

    NurbsSurfaceFitOptions opts;
    opts.useApproximation = true;
    opts.controlPointsU = 6;
    opts.controlPointsV = 5;
    NurbsSurface surface = NurbsSurfaceFit::fit(grid, nu, nv, opts);
    EXPECT_TRUE(surface.isValid());
    EXPECT_EQ(surface.controlPointCountU(), 6);
    EXPECT_EQ(surface.controlPointCountV(), 5);
}

TEST(NurbsSurfaceFit, ApproximationUniformParams) {
    int32_t nu = 10;
    int32_t nv = 8;
    auto grid = makePlanarGrid(nu, nv);

    NurbsSurfaceFitOptions opts;
    opts.useApproximation = true;
    opts.uniformParams = true;
    NurbsSurface surface = NurbsSurfaceFit::fit(grid, nu, nv, opts);
    EXPECT_TRUE(surface.isValid());
    EXPECT_EQ(surface.controlPointCountU(), nu / 2);
    EXPECT_EQ(surface.controlPointCountV(), nv / 2);
}
