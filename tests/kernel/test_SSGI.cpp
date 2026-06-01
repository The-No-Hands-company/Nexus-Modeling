// Null-backend tests for the screen-space global illumination pass (v0.12 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct SSGIFixture : public ::testing::Test {
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

TEST_F(SSGIFixture, SSGISettingsDefaultValues) {
    SSGISettings ssgi;
    EXPECT_FALSE   (ssgi.enabled);
    EXPECT_EQ      (ssgi.rayCount,     4u);
    EXPECT_FLOAT_EQ(ssgi.rayLength,    2.f);
    EXPECT_FLOAT_EQ(ssgi.maxRoughness, 0.8f);
}

TEST_F(SSGIFixture, SetAndGetSSGISettings) {
    Renderer r(*ctx, *sc);
    SSGISettings ssgi;
    ssgi.enabled      = true;
    ssgi.rayCount     = 8;
    ssgi.rayLength    = 3.5f;
    ssgi.maxRoughness = 0.6f;
    r.setSSGISettings(ssgi);
    EXPECT_TRUE    (r.ssgiSettings().enabled);
    EXPECT_EQ      (r.ssgiSettings().rayCount,     8u);
    EXPECT_FLOAT_EQ(r.ssgiSettings().rayLength,    3.5f);
    EXPECT_FLOAT_EQ(r.ssgiSettings().maxRoughness, 0.6f);
}

TEST_F(SSGIFixture, EnableSSGIFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableSSGI);
}

TEST_F(SSGIFixture, SSGIActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSGI = true;
    r.applySettings(s);
    SSGISettings ssgi;
    ssgi.enabled = true;
    r.setSSGISettings(ssgi);
    EXPECT_TRUE(renderOnce(r).ssgiActive);
}

TEST_F(SSGIFixture, SSGIInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSGI = false;
    r.applySettings(s);
    SSGISettings ssgi;
    ssgi.enabled = true;
    r.setSSGISettings(ssgi);
    EXPECT_FALSE(renderOnce(r).ssgiActive);
}

TEST_F(SSGIFixture, SSGIInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSGI = true;
    r.applySettings(s);
    SSGISettings ssgi;
    ssgi.enabled = false;
    r.setSSGISettings(ssgi);
    EXPECT_FALSE(renderOnce(r).ssgiActive);
}

TEST_F(SSGIFixture, SSGIRayCountReflectsResolutionAndSettings) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSGI = true;
    r.applySettings(s);
    SSGISettings ssgi;
    ssgi.enabled   = true;
    ssgi.rayCount  = 8;
    r.setSSGISettings(ssgi);
    // 640 × 480 × 8 = 2457600
    EXPECT_EQ(renderOnce(r).ssgiRayCount, 640u * 480u * 8u);
}

TEST_F(SSGIFixture, SSGIRayCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSGI = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).ssgiRayCount, 0u);
}
