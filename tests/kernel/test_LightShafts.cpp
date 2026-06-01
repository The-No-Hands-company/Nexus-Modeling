// Null-backend tests for the light shaft (god ray) radial blur pass (v0.13 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct LightShaftsFixture : public ::testing::Test {
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

TEST_F(LightShaftsFixture, LightShaftSettingsDefaultValues) {
    LightShaftSettings ls;
    EXPECT_FALSE   (ls.enabled);
    EXPECT_EQ      (ls.sampleCount, 64u);
    EXPECT_FLOAT_EQ(ls.decay,       0.97f);
    EXPECT_FLOAT_EQ(ls.exposure,    0.3f);
}

TEST_F(LightShaftsFixture, SetAndGetLightShaftSettings) {
    Renderer r(*ctx, *sc);
    LightShaftSettings ls;
    ls.enabled     = true;
    ls.sampleCount = 128;
    ls.decay       = 0.95f;
    ls.exposure    = 0.5f;
    r.setLightShaftSettings(ls);
    EXPECT_TRUE    (r.lightShaftSettings().enabled);
    EXPECT_EQ      (r.lightShaftSettings().sampleCount, 128u);
    EXPECT_FLOAT_EQ(r.lightShaftSettings().decay,       0.95f);
    EXPECT_FLOAT_EQ(r.lightShaftSettings().exposure,    0.5f);
}

TEST_F(LightShaftsFixture, EnableLightShaftsFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableLightShafts);
}

TEST_F(LightShaftsFixture, LightShaftsActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLightShafts = true;
    r.applySettings(s);
    LightShaftSettings ls;
    ls.enabled = true;
    r.setLightShaftSettings(ls);
    EXPECT_TRUE(renderOnce(r).lightShaftsActive);
}

TEST_F(LightShaftsFixture, LightShaftsInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLightShafts = false;
    r.applySettings(s);
    LightShaftSettings ls;
    ls.enabled = true;
    r.setLightShaftSettings(ls);
    EXPECT_FALSE(renderOnce(r).lightShaftsActive);
}

TEST_F(LightShaftsFixture, LightShaftsInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLightShafts = true;
    r.applySettings(s);
    LightShaftSettings ls;
    ls.enabled = false;
    r.setLightShaftSettings(ls);
    EXPECT_FALSE(renderOnce(r).lightShaftsActive);
}

TEST_F(LightShaftsFixture, LightShaftSampleCountReflectsSettings) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLightShafts = true;
    r.applySettings(s);
    LightShaftSettings ls;
    ls.enabled     = true;
    ls.sampleCount = 32;
    r.setLightShaftSettings(ls);
    EXPECT_EQ(renderOnce(r).lightShaftSampleCount, 32u);
}

TEST_F(LightShaftsFixture, LightShaftSampleCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLightShafts = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).lightShaftSampleCount, 0u);
}
