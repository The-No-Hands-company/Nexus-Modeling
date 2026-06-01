// Null-backend tests for the decal projection pass (v0.12 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct DecalRenderingFixture : public ::testing::Test {
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

TEST_F(DecalRenderingFixture, DecalSettingsDefaultValues) {
    DecalSettings ds;
    EXPECT_FALSE(ds.enabled);
    EXPECT_EQ   (ds.maxDecals,       256u);
    EXPECT_EQ   (ds.atlasResolution, 2048u);
}

TEST_F(DecalRenderingFixture, SetAndGetDecalSettings) {
    Renderer r(*ctx, *sc);
    DecalSettings ds;
    ds.enabled         = true;
    ds.maxDecals       = 64;
    ds.atlasResolution = 1024;
    r.setDecalSettings(ds);
    EXPECT_TRUE(r.decalSettings().enabled);
    EXPECT_EQ  (r.decalSettings().maxDecals,       64u);
    EXPECT_EQ  (r.decalSettings().atlasResolution, 1024u);
}

TEST_F(DecalRenderingFixture, EnableDecalsFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableDecals);
}

TEST_F(DecalRenderingFixture, DecalsActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDecals = true;
    r.applySettings(s);
    DecalSettings ds;
    ds.enabled = true;
    r.setDecalSettings(ds);
    EXPECT_TRUE(renderOnce(r).decalsActive);
}

TEST_F(DecalRenderingFixture, DecalsInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDecals = false;
    r.applySettings(s);
    DecalSettings ds;
    ds.enabled = true;
    r.setDecalSettings(ds);
    EXPECT_FALSE(renderOnce(r).decalsActive);
}

TEST_F(DecalRenderingFixture, DecalsInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDecals = true;
    r.applySettings(s);
    DecalSettings ds;
    ds.enabled = false;
    r.setDecalSettings(ds);
    EXPECT_FALSE(renderOnce(r).decalsActive);
}

TEST_F(DecalRenderingFixture, DecalCountPopulatedWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDecals = true;
    r.applySettings(s);
    DecalSettings ds;
    ds.enabled   = true;
    ds.maxDecals = 128;
    r.setDecalSettings(ds);
    EXPECT_GT(renderOnce(r).decalCount, 0u);
}

TEST_F(DecalRenderingFixture, DecalCountReflectsMaxDecals) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDecals = true;
    r.applySettings(s);
    DecalSettings ds;
    ds.enabled   = true;
    ds.maxDecals = 32;
    r.setDecalSettings(ds);
    EXPECT_EQ(renderOnce(r).decalCount, 32u);
}

TEST_F(DecalRenderingFixture, DecalCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDecals = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).decalCount, 0u);
}
