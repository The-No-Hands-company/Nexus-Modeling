// Null-backend tests for the SSS separable blur pass (v0.11 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct SSSFixture : public ::testing::Test {
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

TEST_F(SSSFixture, SSSSettingsDefaultValues) {
    SSSSettings sss;
    EXPECT_FALSE   (sss.enabled);
    EXPECT_FLOAT_EQ(sss.scatterRadius, 1.f);
    EXPECT_EQ      (sss.profileCount,  1u);
    EXPECT_EQ      (sss.blurPasses,    2u);
}

TEST_F(SSSFixture, SetAndGetSSSSettings) {
    Renderer r(*ctx, *sc);
    SSSSettings sss;
    sss.enabled       = true;
    sss.scatterRadius = 2.5f;
    sss.profileCount  = 3;
    sss.blurPasses    = 4;
    r.setSSSSettings(sss);
    EXPECT_TRUE    (r.sssSettings().enabled);
    EXPECT_FLOAT_EQ(r.sssSettings().scatterRadius, 2.5f);
    EXPECT_EQ      (r.sssSettings().profileCount,  3u);
    EXPECT_EQ      (r.sssSettings().blurPasses,    4u);
}

TEST_F(SSSFixture, EnableSSSFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableSSS);
}

TEST_F(SSSFixture, SSSActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSS = true;
    r.applySettings(s);
    SSSSettings sss;
    sss.enabled = true;
    r.setSSSSettings(sss);
    EXPECT_TRUE(renderOnce(r).sssActive);
}

TEST_F(SSSFixture, SSSInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSS = false;
    r.applySettings(s);
    SSSSettings sss;
    sss.enabled = true;
    r.setSSSSettings(sss);
    EXPECT_FALSE(renderOnce(r).sssActive);
}

TEST_F(SSSFixture, SSSInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSS = true;
    r.applySettings(s);
    SSSSettings sss;
    sss.enabled = false;
    r.setSSSSettings(sss);
    EXPECT_FALSE(renderOnce(r).sssActive);
}

TEST_F(SSSFixture, SSSBlurPassCountReflectsSettings) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSS = true;
    r.applySettings(s);
    SSSSettings sss;
    sss.enabled    = true;
    sss.blurPasses = 3; // 3 pairs = 6 dispatches
    r.setSSSSettings(sss);
    EXPECT_EQ(renderOnce(r).sssBlurPasses, 6u);
}

TEST_F(SSSFixture, SSSBlurPassCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSS = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).sssBlurPasses, 0u);
}
