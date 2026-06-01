// Null-backend tests for the tiled deferred lighting pass (v0.11 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct TiledLightingFixture : public ::testing::Test {
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

TEST_F(TiledLightingFixture, TiledLightingSettingsDefaultValues) {
    TiledLightingSettings tl;
    EXPECT_FALSE(tl.enabled);
    EXPECT_EQ   (tl.tileSize,         16u);
    EXPECT_EQ   (tl.maxLightsPerTile, 256u);
}

TEST_F(TiledLightingFixture, SetAndGetTiledLightingSettings) {
    Renderer r(*ctx, *sc);
    TiledLightingSettings tl;
    tl.enabled          = true;
    tl.tileSize         = 32;
    tl.maxLightsPerTile = 128;
    r.setTiledLightingSettings(tl);
    EXPECT_TRUE(r.tiledLightingSettings().enabled);
    EXPECT_EQ  (r.tiledLightingSettings().tileSize,         32u);
    EXPECT_EQ  (r.tiledLightingSettings().maxLightsPerTile, 128u);
}

TEST_F(TiledLightingFixture, EnableTiledLightingFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableTiledLighting);
}

TEST_F(TiledLightingFixture, TiledLightingActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableTiledLighting = true;
    r.applySettings(s);
    TiledLightingSettings tl;
    tl.enabled = true;
    r.setTiledLightingSettings(tl);
    EXPECT_TRUE(renderOnce(r).tiledLightingActive);
}

TEST_F(TiledLightingFixture, TiledLightingInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableTiledLighting = false;
    r.applySettings(s);
    TiledLightingSettings tl;
    tl.enabled = true;
    r.setTiledLightingSettings(tl);
    EXPECT_FALSE(renderOnce(r).tiledLightingActive);
}

TEST_F(TiledLightingFixture, TiledLightingInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableTiledLighting = true;
    r.applySettings(s);
    TiledLightingSettings tl;
    tl.enabled = false;
    r.setTiledLightingSettings(tl);
    EXPECT_FALSE(renderOnce(r).tiledLightingActive);
}

TEST_F(TiledLightingFixture, LightTileCountPopulatedWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableTiledLighting = true;
    r.applySettings(s);
    TiledLightingSettings tl;
    tl.enabled  = true;
    tl.tileSize = 16;
    r.setTiledLightingSettings(tl);
    // 640/16=40, 480/16=30 → 1200 tiles
    EXPECT_EQ(renderOnce(r).lightTileCount, 40u * 30u);
}

TEST_F(TiledLightingFixture, MaxLightsPerTileReflectsSettings) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableTiledLighting = true;
    r.applySettings(s);
    TiledLightingSettings tl;
    tl.enabled          = true;
    tl.maxLightsPerTile = 64;
    r.setTiledLightingSettings(tl);
    EXPECT_EQ(renderOnce(r).maxLightsPerTile, 64u);
}

TEST_F(TiledLightingFixture, LightTileCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableTiledLighting = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).lightTileCount, 0u);
}
