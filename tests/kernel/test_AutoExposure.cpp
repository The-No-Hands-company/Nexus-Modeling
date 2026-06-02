// Null-backend tests for Auto-Exposure / Eye Adaptation (v0.17 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct AutoExposureFixture : public ::testing::Test {
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

TEST_F(AutoExposureFixture, AutoExposureSettingsDefaultDisabled) {
    AutoExposureSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(AutoExposureFixture, AutoExposureSettingsDefaultMinEV) {
    AutoExposureSettings s;
    EXPECT_FLOAT_EQ(s.minEV, -4.f);
}

TEST_F(AutoExposureFixture, AutoExposureSettingsDefaultMaxEV) {
    AutoExposureSettings s;
    EXPECT_FLOAT_EQ(s.maxEV, 4.f);
}

TEST_F(AutoExposureFixture, AutoExposureSettingsDefaultTargetLuminance) {
    AutoExposureSettings s;
    EXPECT_FLOAT_EQ(s.targetLuminance, 0.18f);
}

TEST_F(AutoExposureFixture, SetAndGetAutoExposureSettings) {
    Renderer r(*ctx, *sc);
    AutoExposureSettings s;
    s.enabled         = true;
    s.minEV           = -2.f;
    s.maxEV           = 6.f;
    s.adaptationSpeed = 2.f;
    s.targetLuminance = 0.25f;
    r.setAutoExposureSettings(s);
    EXPECT_TRUE     (r.autoExposureSettings().enabled);
    EXPECT_FLOAT_EQ (r.autoExposureSettings().minEV,           -2.f);
    EXPECT_FLOAT_EQ (r.autoExposureSettings().maxEV,            6.f);
    EXPECT_FLOAT_EQ (r.autoExposureSettings().adaptationSpeed,  2.f);
    EXPECT_FLOAT_EQ (r.autoExposureSettings().targetLuminance,  0.25f);
}

TEST_F(AutoExposureFixture, AutoExposureGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    AutoExposureSettings s;
    s.enabled         = true;
    s.targetLuminance = 0.18f;
    r.setAutoExposureSettings(s);
    RendererSettings rs;
    rs.enableAutoExposure = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE    (stats.autoExposureActive);
    EXPECT_FLOAT_EQ (stats.autoExposureEV, 0.f);
}

TEST_F(AutoExposureFixture, AutoExposureGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    AutoExposureSettings s;
    s.enabled         = true;
    s.targetLuminance = 0.18f;
    r.setAutoExposureSettings(s);
    RendererSettings rs;
    rs.enableAutoExposure = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.autoExposureActive);
}

TEST_F(AutoExposureFixture, AutoExposureEVReflectsTargetLuminance) {
    Renderer r(*ctx, *sc);
    AutoExposureSettings s;
    s.enabled         = true;
    s.targetLuminance = 0.5f;
    r.setAutoExposureSettings(s);
    RendererSettings rs;
    rs.enableAutoExposure = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FLOAT_EQ(stats.autoExposureEV, 0.5f);
}

TEST_F(AutoExposureFixture, AutoExposureInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    AutoExposureSettings s;
    s.enabled         = false;
    s.targetLuminance = 0.18f;
    r.setAutoExposureSettings(s);
    RendererSettings rs;
    rs.enableAutoExposure = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.autoExposureActive);
}
