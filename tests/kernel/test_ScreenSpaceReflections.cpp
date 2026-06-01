// Null-backend tests for the SSR compute pass (v0.8 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct SSRFixture : public ::testing::Test {
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

TEST_F(SSRFixture, SSRSettingsDefaultValues) {
    SSRSettings ssr;
    EXPECT_EQ      (ssr.maxRaySteps,  64u);
    EXPECT_FLOAT_EQ(ssr.stepSize,     0.1f);
    EXPECT_FLOAT_EQ(ssr.thickness,    0.05f);
    EXPECT_FLOAT_EQ(ssr.fadeDistance, 10.f);
}

TEST_F(SSRFixture, SetAndGetSSRSettings) {
    Renderer r(*ctx, *sc);
    SSRSettings ssr;
    ssr.maxRaySteps  = 128;
    ssr.stepSize     = 0.05f;
    ssr.thickness    = 0.02f;
    ssr.fadeDistance = 20.f;
    r.setSSRSettings(ssr);
    EXPECT_EQ      (r.ssrSettings().maxRaySteps,  128u);
    EXPECT_FLOAT_EQ(r.ssrSettings().stepSize,     0.05f);
    EXPECT_FLOAT_EQ(r.ssrSettings().thickness,    0.02f);
    EXPECT_FLOAT_EQ(r.ssrSettings().fadeDistance, 20.f);
}

TEST_F(SSRFixture, EnableSSRFlagDefaultFalse) {
    RendererSettings settings;
    EXPECT_FALSE(settings.enableSSR);
}

TEST_F(SSRFixture, SSRActiveWhenEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSR = true;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.ssrActive);
}

TEST_F(SSRFixture, SSRInactiveWhenDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSR = false;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.ssrActive);
}

TEST_F(SSRFixture, SSRRayCountPopulatedWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSR = true;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_GT(stats.ssrRayCount, 0u);
}

TEST_F(SSRFixture, SSRRayCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableSSR = false;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.ssrRayCount, 0u);
}

TEST_F(SSRFixture, SSRSettingsRoundTripIndependent) {
    Renderer r(*ctx, *sc);
    SSRSettings a;
    a.maxRaySteps = 32;
    r.setSSRSettings(a);
    SSRSettings b;
    b.maxRaySteps = 256;
    r.setSSRSettings(b);
    EXPECT_EQ(r.ssrSettings().maxRaySteps, 256u);
}
