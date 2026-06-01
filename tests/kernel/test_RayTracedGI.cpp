// Null-backend tests for the hardware ray-traced GI pass (v0.13 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct RTGIFixture : public ::testing::Test {
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

TEST_F(RTGIFixture, RTGISettingsDefaultValues) {
    RTGISettings rtgi;
    EXPECT_FALSE(rtgi.enabled);
    EXPECT_EQ   (rtgi.raysPerPixel, 2u);
    EXPECT_EQ   (rtgi.maxBounces,   1u);
    EXPECT_TRUE (rtgi.denoised);
}

TEST_F(RTGIFixture, SetAndGetRTGISettings) {
    Renderer r(*ctx, *sc);
    RTGISettings rtgi;
    rtgi.enabled      = true;
    rtgi.raysPerPixel = 4;
    rtgi.maxBounces   = 2;
    rtgi.denoised     = false;
    r.setRTGISettings(rtgi);
    EXPECT_TRUE (r.rtgiSettings().enabled);
    EXPECT_EQ   (r.rtgiSettings().raysPerPixel, 4u);
    EXPECT_EQ   (r.rtgiSettings().maxBounces,   2u);
    EXPECT_FALSE(r.rtgiSettings().denoised);
}

TEST_F(RTGIFixture, EnableRTGIFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableRTGI);
}

TEST_F(RTGIFixture, RTGIActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = true;
    r.applySettings(s);
    RTGISettings rtgi;
    rtgi.enabled = true;
    r.setRTGISettings(rtgi);
    EXPECT_TRUE(renderOnce(r).rtgiActive);
}

TEST_F(RTGIFixture, RTGIInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = false;
    r.applySettings(s);
    RTGISettings rtgi;
    rtgi.enabled = true;
    r.setRTGISettings(rtgi);
    EXPECT_FALSE(renderOnce(r).rtgiActive);
}

TEST_F(RTGIFixture, RTGIInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = true;
    r.applySettings(s);
    RTGISettings rtgi;
    rtgi.enabled = false;
    r.setRTGISettings(rtgi);
    EXPECT_FALSE(renderOnce(r).rtgiActive);
}

TEST_F(RTGIFixture, RTGIRaysDispatchedReflectsResolutionAndRaysPerPixel) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = true;
    r.applySettings(s);
    RTGISettings rtgi;
    rtgi.enabled      = true;
    rtgi.raysPerPixel = 4;
    r.setRTGISettings(rtgi);
    // 640 × 480 × 4 = 1228800
    EXPECT_EQ(renderOnce(r).rtgiRaysDispatched, 640u * 480u * 4u);
}

TEST_F(RTGIFixture, RTGIRaysDispatchedZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).rtgiRaysDispatched, 0u);
}
