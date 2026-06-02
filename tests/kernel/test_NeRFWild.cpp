// Null-backend tests for NeRF-in-the-Wild appearance + transient rendering (v0.24 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct NeRFWildFixture : public ::testing::Test {
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

TEST_F(NeRFWildFixture, NeRFWildSettingsDefaultDisabled) {
    NeRFWildSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(NeRFWildFixture, NeRFWildSettingsDefaultAppearanceEmbeddingDim) {
    NeRFWildSettings s;
    EXPECT_EQ(s.appearanceEmbeddingDim, 48u);
}

TEST_F(NeRFWildFixture, NeRFWildSettingsDefaultTransientLayers) {
    NeRFWildSettings s;
    EXPECT_EQ(s.transientLayers, 4u);
}

TEST_F(NeRFWildFixture, NeRFWildSettingsDefaultDistractorThreshold) {
    NeRFWildSettings s;
    EXPECT_FLOAT_EQ(s.distractorThreshold, 0.5f);
}

TEST_F(NeRFWildFixture, SetAndGetNeRFWildSettings) {
    Renderer r(*ctx, *sc);
    NeRFWildSettings s;
    s.enabled                = true;
    s.appearanceEmbeddingDim = 64;
    s.transientLayers        = 6;
    s.distractorThreshold    = 0.3f;
    r.setNeRFWildSettings(s);
    EXPECT_TRUE     (r.neRFWildSettings().enabled);
    EXPECT_EQ       (r.neRFWildSettings().appearanceEmbeddingDim, 64u);
    EXPECT_EQ       (r.neRFWildSettings().transientLayers,        6u);
    EXPECT_FLOAT_EQ (r.neRFWildSettings().distractorThreshold,    0.3f);
}

TEST_F(NeRFWildFixture, NeRFWildGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    NeRFWildSettings s;
    s.enabled         = true;
    s.transientLayers = 4;
    r.setNeRFWildSettings(s);
    RendererSettings rs;
    rs.enableNeRFWild = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.neRFWildActive);
    EXPECT_EQ   (stats.neRFWildTransientPasses, 0u);
}

TEST_F(NeRFWildFixture, NeRFWildGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    NeRFWildSettings s;
    s.enabled         = true;
    s.transientLayers = 4;
    r.setNeRFWildSettings(s);
    RendererSettings rs;
    rs.enableNeRFWild = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.neRFWildActive);
    // transientPasses = 1 (appearance pass) + transientLayers
    EXPECT_EQ  (stats.neRFWildTransientPasses, 5u);
}

TEST_F(NeRFWildFixture, NeRFWildTransientPassesReflectLayers) {
    Renderer r(*ctx, *sc);
    NeRFWildSettings s;
    s.enabled         = true;
    s.transientLayers = 2;
    r.setNeRFWildSettings(s);
    RendererSettings rs;
    rs.enableNeRFWild = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.neRFWildTransientPasses, 3u);
}

TEST_F(NeRFWildFixture, NeRFWildInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    NeRFWildSettings s;
    s.enabled         = false;
    s.transientLayers = 4;
    r.setNeRFWildSettings(s);
    RendererSettings rs;
    rs.enableNeRFWild = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.neRFWildActive);
}
