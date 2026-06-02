// Null-backend tests for NeRF Relighting / Material Decomposition (v0.25 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct NeRFRelightingFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain>    sc;

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

} // namespace

TEST_F(NeRFRelightingFixture, NeRFRelightingSettingsDefaultDisabled) {
    NeRFRelightingSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(NeRFRelightingFixture, NeRFRelightingSettingsDefaultDecompositionLayers) {
    NeRFRelightingSettings s;
    EXPECT_EQ(s.decompositionLayers, 4u);
}

TEST_F(NeRFRelightingFixture, NeRFRelightingSettingsDefaultEnvMapResolution) {
    NeRFRelightingSettings s;
    EXPECT_EQ(s.envMapResolution, 256u);
}

TEST_F(NeRFRelightingFixture, NeRFRelightingSettingsDefaultSpecularSamples) {
    NeRFRelightingSettings s;
    EXPECT_EQ(s.specularSamples, 16u);
}

TEST_F(NeRFRelightingFixture, SetAndGetNeRFRelightingSettings) {
    Renderer r(*ctx, *sc);
    NeRFRelightingSettings s;
    s.enabled              = true;
    s.decompositionLayers  = 6;
    s.envMapResolution     = 512;
    s.specularSamples      = 32;
    r.setNeRFRelightingSettings(s);
    EXPECT_TRUE(r.neRFRelightingSettings().enabled);
    EXPECT_EQ  (r.neRFRelightingSettings().decompositionLayers, 6u);
    EXPECT_EQ  (r.neRFRelightingSettings().envMapResolution,    512u);
    EXPECT_EQ  (r.neRFRelightingSettings().specularSamples,     32u);
}

TEST_F(NeRFRelightingFixture, NeRFRelightingGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    NeRFRelightingSettings s;
    s.enabled         = true;
    s.specularSamples = 16;
    r.setNeRFRelightingSettings(s);
    RendererSettings rs;
    rs.enableNeRFRelighting = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.neRFRelightingActive);
    EXPECT_EQ   (stats.neRFRelightingSampleCount, 0u);
}

TEST_F(NeRFRelightingFixture, NeRFRelightingGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    NeRFRelightingSettings s;
    s.enabled         = true;
    s.specularSamples = 16;
    r.setNeRFRelightingSettings(s);
    RendererSettings rs;
    rs.enableNeRFRelighting = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.neRFRelightingActive);
    EXPECT_EQ  (stats.neRFRelightingSampleCount, 16u);
}

TEST_F(NeRFRelightingFixture, NeRFRelightingSampleCountReflectedInStat) {
    Renderer r(*ctx, *sc);
    NeRFRelightingSettings s;
    s.enabled         = true;
    s.specularSamples = 64;
    r.setNeRFRelightingSettings(s);
    RendererSettings rs;
    rs.enableNeRFRelighting = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.neRFRelightingSampleCount, 64u);
}

TEST_F(NeRFRelightingFixture, NeRFRelightingInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    NeRFRelightingSettings s;
    s.enabled         = false;
    s.specularSamples = 16;
    r.setNeRFRelightingSettings(s);
    RendererSettings rs;
    rs.enableNeRFRelighting = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.neRFRelightingActive);
}
