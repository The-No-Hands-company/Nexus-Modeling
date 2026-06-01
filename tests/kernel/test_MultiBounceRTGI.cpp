// Null-backend tests for multi-bounce RTGI extension (v0.14 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct MultiBounceRTGIFixture : public ::testing::Test {
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

TEST_F(MultiBounceRTGIFixture, RTGISettingsHaveMultiBounceDefaults) {
    RTGISettings rtgi;
    EXPECT_FLOAT_EQ(rtgi.multiBounceFeedback, 0.5f);
    EXPECT_FLOAT_EQ(rtgi.temporalAlpha,       0.1f);
}

TEST_F(MultiBounceRTGIFixture, SetAndGetMultiBounceFields) {
    Renderer r(*ctx, *sc);
    RTGISettings rtgi;
    rtgi.enabled             = true;
    rtgi.multiBounceFeedback = 0.7f;
    rtgi.temporalAlpha       = 0.2f;
    r.setRTGISettings(rtgi);
    EXPECT_FLOAT_EQ(r.rtgiSettings().multiBounceFeedback, 0.7f);
    EXPECT_FLOAT_EQ(r.rtgiSettings().temporalAlpha,       0.2f);
}

TEST_F(MultiBounceRTGIFixture, RTGIBounceCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).rtgiBounceCount, 0u);
}

TEST_F(MultiBounceRTGIFixture, RTGIBounceCountEqualsMaxBouncesWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = true;
    r.applySettings(s);
    RTGISettings rtgi;
    rtgi.enabled     = true;
    rtgi.maxBounces  = 3;
    r.setRTGISettings(rtgi);
    EXPECT_EQ(renderOnce(r).rtgiBounceCount, 3u);
}

TEST_F(MultiBounceRTGIFixture, SingleBounceStillWorksAfterExtension) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = true;
    r.applySettings(s);
    RTGISettings rtgi;
    rtgi.enabled    = true;
    rtgi.maxBounces = 1;
    r.setRTGISettings(rtgi);
    const auto stats = renderOnce(r);
    EXPECT_TRUE(stats.rtgiActive);
    EXPECT_EQ  (stats.rtgiBounceCount, 1u);
}

TEST_F(MultiBounceRTGIFixture, RTGIRaysDispatchedUnchangedByMultiBounce) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = true;
    r.applySettings(s);
    RTGISettings rtgi;
    rtgi.enabled      = true;
    rtgi.raysPerPixel = 2;
    rtgi.maxBounces   = 4;
    r.setRTGISettings(rtgi);
    // raysDispatched is per-pixel rays, not total; maxBounces affects bounceCount
    EXPECT_EQ(renderOnce(r).rtgiRaysDispatched, 640u * 480u * 2u);
}

TEST_F(MultiBounceRTGIFixture, MultiBounceInactiveWhenRTGIDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableRTGI = false;
    r.applySettings(s);
    RTGISettings rtgi;
    rtgi.enabled    = true;
    rtgi.maxBounces = 3;
    r.setRTGISettings(rtgi);
    EXPECT_EQ(renderOnce(r).rtgiBounceCount, 0u);
}

TEST_F(MultiBounceRTGIFixture, MultiBounceFieldsRoundTripWithOtherFields) {
    Renderer r(*ctx, *sc);
    RTGISettings rtgi;
    rtgi.raysPerPixel         = 4;
    rtgi.maxBounces           = 2;
    rtgi.denoised             = false;
    rtgi.multiBounceFeedback  = 0.3f;
    rtgi.temporalAlpha        = 0.05f;
    r.setRTGISettings(rtgi);
    EXPECT_EQ      (r.rtgiSettings().raysPerPixel,        4u);
    EXPECT_EQ      (r.rtgiSettings().maxBounces,          2u);
    EXPECT_FALSE   (r.rtgiSettings().denoised);
    EXPECT_FLOAT_EQ(r.rtgiSettings().multiBounceFeedback, 0.3f);
    EXPECT_FLOAT_EQ(r.rtgiSettings().temporalAlpha,       0.05f);
}
