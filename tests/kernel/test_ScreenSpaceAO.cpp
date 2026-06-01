// Null-backend tests for the SSAO compute pass (v0.8 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct AOFixture : public ::testing::Test {
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

TEST_F(AOFixture, AOSettingsDefaultValues) {
    AOSettings ao;
    EXPECT_FLOAT_EQ(ao.radius,      0.5f);
    EXPECT_FLOAT_EQ(ao.bias,        0.025f);
    EXPECT_EQ      (ao.sampleCount, 16u);
    EXPECT_EQ      (ao.blurPasses,  2u);
}

TEST_F(AOFixture, SetAndGetAOSettings) {
    Renderer r(*ctx, *sc);
    AOSettings ao;
    ao.radius      = 1.f;
    ao.bias        = 0.01f;
    ao.sampleCount = 32;
    ao.blurPasses  = 4;
    r.setAOSettings(ao);
    EXPECT_FLOAT_EQ(r.aoSettings().radius,      1.f);
    EXPECT_FLOAT_EQ(r.aoSettings().bias,        0.01f);
    EXPECT_EQ      (r.aoSettings().sampleCount, 32u);
    EXPECT_EQ      (r.aoSettings().blurPasses,  4u);
}

TEST_F(AOFixture, EnableAOFlagDefaultTrue) {
    RendererSettings settings;
    EXPECT_TRUE(settings.enableAO);
}

TEST_F(AOFixture, AOActiveWhenEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableAO = true;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.aoActive);
}

TEST_F(AOFixture, AOInactiveWhenDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableAO = false;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.aoActive);
}

TEST_F(AOFixture, AOSampleCountPopulatedWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableAO = true;
    r.applySettings(s);
    AOSettings ao;
    ao.sampleCount = 8;
    r.setAOSettings(ao);
    FrameStats stats = renderOnce(r);
    EXPECT_GT(stats.aoSampleCount, 0u);
}

TEST_F(AOFixture, AOSampleCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableAO = false;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.aoSampleCount, 0u);
}

TEST_F(AOFixture, AOSettingsRoundTripIndependent) {
    Renderer r(*ctx, *sc);
    AOSettings ao1;
    ao1.radius      = 0.3f;
    ao1.sampleCount = 8;
    r.setAOSettings(ao1);
    AOSettings ao2;
    ao2.radius      = 0.7f;
    ao2.sampleCount = 64;
    r.setAOSettings(ao2);
    EXPECT_FLOAT_EQ(r.aoSettings().radius,      0.7f);
    EXPECT_EQ      (r.aoSettings().sampleCount, 64u);
}
