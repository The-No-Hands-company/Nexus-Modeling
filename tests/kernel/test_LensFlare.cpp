// Null-backend tests for the lens flare and anamorphic streak pass (v0.15 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct LensFlareFixture : public ::testing::Test {
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

TEST_F(LensFlareFixture, LensFlareSettingsDefaultValues) {
    LensFlareSettings lf;
    EXPECT_FALSE   (lf.enabled);
    EXPECT_EQ      (lf.ghostCount,   4u);
    EXPECT_FLOAT_EQ(lf.streakLength, 0.8f);
    EXPECT_FLOAT_EQ(lf.threshold,    1.f);
    EXPECT_FLOAT_EQ(lf.intensity,    0.15f);
}

TEST_F(LensFlareFixture, SetAndGetLensFlareSettings) {
    Renderer r(*ctx, *sc);
    LensFlareSettings lf;
    lf.enabled      = true;
    lf.ghostCount   = 6;
    lf.streakLength = 0.6f;
    lf.threshold    = 2.f;
    lf.intensity    = 0.25f;
    r.setLensFlareSettings(lf);
    EXPECT_TRUE    (r.lensFlareSettings().enabled);
    EXPECT_EQ      (r.lensFlareSettings().ghostCount,   6u);
    EXPECT_FLOAT_EQ(r.lensFlareSettings().streakLength, 0.6f);
    EXPECT_FLOAT_EQ(r.lensFlareSettings().threshold,    2.f);
    EXPECT_FLOAT_EQ(r.lensFlareSettings().intensity,    0.25f);
}

TEST_F(LensFlareFixture, EnableLensFlareFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableLensFlare);
}

TEST_F(LensFlareFixture, LensFlareActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLensFlare = true;
    r.applySettings(s);
    LensFlareSettings lf;
    lf.enabled = true;
    r.setLensFlareSettings(lf);
    EXPECT_TRUE(renderOnce(r).lensFlareActive);
}

TEST_F(LensFlareFixture, LensFlareInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLensFlare = false;
    r.applySettings(s);
    LensFlareSettings lf;
    lf.enabled = true;
    r.setLensFlareSettings(lf);
    EXPECT_FALSE(renderOnce(r).lensFlareActive);
}

TEST_F(LensFlareFixture, LensFlareInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLensFlare = true;
    r.applySettings(s);
    LensFlareSettings lf;
    lf.enabled = false;
    r.setLensFlareSettings(lf);
    EXPECT_FALSE(renderOnce(r).lensFlareActive);
}

TEST_F(LensFlareFixture, GhostCountReflectsSettings) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLensFlare = true;
    r.applySettings(s);
    LensFlareSettings lf;
    lf.enabled    = true;
    lf.ghostCount = 8;
    r.setLensFlareSettings(lf);
    EXPECT_EQ(renderOnce(r).lensFlareGhostCount, 8u);
}

TEST_F(LensFlareFixture, GhostCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableLensFlare = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).lensFlareGhostCount, 0u);
}
