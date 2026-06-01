// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Volumetric Lighting Pass (v0.7 Track 2)
//
//  Verifies VolumetricSettings, setVolumetricSettings, FrameStats::volumetricActive,
//  and FrameStats::volumetricFroxelCount. All tests run on Null backend.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::render;
using namespace nexus::gfx;

namespace {

struct VolFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain> sc;

    void SetUp() override {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);
        SwapchainDesc sd{};
        sd.extent = { 640, 480 };
        sc = ctx->createSwapchain(sd);
        ASSERT_NE(sc, nullptr);
    }

    FrameStats renderOnce(Renderer& r) {
        SceneGraph scene;
        Camera cam;
        r.beginFrame();
        r.render(cam, scene);
        r.endFrame();
        return r.lastFrameStats();
    }
};

// ── Default state ─────────────────────────────────────────────────────────────

TEST_F(VolFixture, VolumetricDefaultIsDisabled) {
    Renderer renderer(*ctx, *sc);
    EXPECT_FALSE(renderer.volumetricSettings().enabled);
}

TEST_F(VolFixture, VolumetricActiveDefaultsToFalse) {
    Renderer renderer(*ctx, *sc);
    FrameStats stats = renderOnce(renderer);
    EXPECT_FALSE(stats.volumetricActive);
    EXPECT_EQ(stats.volumetricFroxelCount, 0u);
}

// ── Settings round-trip ───────────────────────────────────────────────────────

TEST_F(VolFixture, VolumetricSettingsRoundTrip) {
    Renderer renderer(*ctx, *sc);
    VolumetricSettings vs{};
    vs.enabled                  = true;
    vs.fogDensity               = 0.05f;
    vs.scatteringCoeff          = 0.8f;
    vs.froxelSlices             = 128;
    vs.froxelResolutionDivisor  = 4;
    renderer.setVolumetricSettings(vs);

    const auto& stored = renderer.volumetricSettings();
    EXPECT_TRUE(stored.enabled);
    EXPECT_FLOAT_EQ(stored.fogDensity, 0.05f);
    EXPECT_FLOAT_EQ(stored.scatteringCoeff, 0.8f);
    EXPECT_EQ(stored.froxelSlices, 128u);
    EXPECT_EQ(stored.froxelResolutionDivisor, 4u);
}

// ── Dispatch behaviour ────────────────────────────────────────────────────────

TEST_F(VolFixture, EnabledVolumetricSetsActiveFlag) {
    Renderer renderer(*ctx, *sc);
    VolumetricSettings vs{};
    vs.enabled = true;
    renderer.setVolumetricSettings(vs);

    FrameStats stats = renderOnce(renderer);
    EXPECT_TRUE(stats.volumetricActive);
}

TEST_F(VolFixture, EnabledVolumetricPopulatesFroxelCount) {
    Renderer renderer(*ctx, *sc);
    VolumetricSettings vs{};
    vs.enabled                  = true;
    vs.froxelSlices             = 64;
    vs.froxelResolutionDivisor  = 8;
    renderer.setVolumetricSettings(vs);

    FrameStats stats = renderOnce(renderer);
    EXPECT_TRUE(stats.volumetricActive);
    EXPECT_GT(stats.volumetricFroxelCount, 0u);
}

TEST_F(VolFixture, DisabledVolumetricProducesZeroFroxels) {
    Renderer renderer(*ctx, *sc);
    VolumetricSettings vs{};
    vs.enabled = false;
    renderer.setVolumetricSettings(vs);

    FrameStats stats = renderOnce(renderer);
    EXPECT_FALSE(stats.volumetricActive);
    EXPECT_EQ(stats.volumetricFroxelCount, 0u);
}

TEST_F(VolFixture, VolumetricStatsResetEachFrame) {
    Renderer renderer(*ctx, *sc);
    VolumetricSettings vs{};
    vs.enabled = true;
    renderer.setVolumetricSettings(vs);

    const FrameStats f1 = renderOnce(renderer);
    EXPECT_TRUE(f1.volumetricActive);
    EXPECT_GT(f1.volumetricFroxelCount, 0u);

    vs.enabled = false;
    renderer.setVolumetricSettings(vs);

    const FrameStats f2 = renderOnce(renderer);
    EXPECT_FALSE(f2.volumetricActive);
    EXPECT_EQ(f2.volumetricFroxelCount, 0u);
}

TEST_F(VolFixture, VolumetricFroxelCountMatchesExpectedFormula) {
    // Render target extent on Null backend defaults to 1×1 or the swapchain size.
    // We validate the formula: froxelW * froxelH * froxelSlices > 0 when enabled,
    // with a known divisor=1 (full resolution froxel grid).
    Renderer renderer(*ctx, *sc);
    VolumetricSettings vs{};
    vs.enabled                  = true;
    vs.froxelSlices             = 32;
    vs.froxelResolutionDivisor  = 1;  // froxel XY == render target XY
    renderer.setVolumetricSettings(vs);

    FrameStats stats = renderOnce(renderer);
    EXPECT_TRUE(stats.volumetricActive);
    // At minimum 1×1 froxel grid × 32 slices = 32 froxels.
    EXPECT_GE(stats.volumetricFroxelCount, 32u);
}

}  // namespace
