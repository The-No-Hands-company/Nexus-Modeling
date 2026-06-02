// Null-backend tests for Histogram-Based Auto White Balance (v0.18 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct AWBFixture : public ::testing::Test {
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

TEST_F(AWBFixture, AutoWhiteBalanceSettingsDefaultDisabled) {
    AutoWhiteBalanceSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(AWBFixture, AutoWhiteBalanceSettingsDefaultMethod) {
    AutoWhiteBalanceSettings s;
    EXPECT_EQ(s.method, AutoWhiteBalanceMethod::GrayWorld);
}

TEST_F(AWBFixture, AutoWhiteBalanceSettingsDefaultStrength) {
    AutoWhiteBalanceSettings s;
    EXPECT_FLOAT_EQ(s.strength, 1.f);
}

TEST_F(AWBFixture, AutoWhiteBalanceSettingsDefaultAdaptationSpeed) {
    AutoWhiteBalanceSettings s;
    EXPECT_FLOAT_EQ(s.adaptationSpeed, 0.5f);
}

TEST_F(AWBFixture, SetAndGetAutoWhiteBalanceSettings) {
    Renderer r(*ctx, *sc);
    AutoWhiteBalanceSettings s;
    s.enabled         = true;
    s.method          = AutoWhiteBalanceMethod::WhitePatch;
    s.strength        = 0.8f;
    s.adaptationSpeed = 1.f;
    r.setAutoWhiteBalanceSettings(s);
    EXPECT_TRUE     (r.autoWhiteBalanceSettings().enabled);
    EXPECT_EQ       (r.autoWhiteBalanceSettings().method,          AutoWhiteBalanceMethod::WhitePatch);
    EXPECT_FLOAT_EQ (r.autoWhiteBalanceSettings().strength,        0.8f);
    EXPECT_FLOAT_EQ (r.autoWhiteBalanceSettings().adaptationSpeed, 1.f);
}

TEST_F(AWBFixture, AWBGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    AutoWhiteBalanceSettings s;
    s.enabled = true;
    r.setAutoWhiteBalanceSettings(s);
    RendererSettings rs;
    rs.enableAutoWhiteBalance = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.autoWhiteBalanceActive);
}

TEST_F(AWBFixture, AWBGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    AutoWhiteBalanceSettings s;
    s.enabled = true;
    r.setAutoWhiteBalanceSettings(s);
    RendererSettings rs;
    rs.enableAutoWhiteBalance = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.autoWhiteBalanceActive);
}

TEST_F(AWBFixture, AWBMethodReflectedInStat_GrayWorld) {
    Renderer r(*ctx, *sc);
    AutoWhiteBalanceSettings s;
    s.enabled = true;
    s.method  = AutoWhiteBalanceMethod::GrayWorld;
    r.setAutoWhiteBalanceSettings(s);
    RendererSettings rs;
    rs.enableAutoWhiteBalance = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.autoWhiteBalanceMethod, AutoWhiteBalanceMethod::GrayWorld);
}

TEST_F(AWBFixture, AWBInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    AutoWhiteBalanceSettings s;
    s.enabled = false;
    r.setAutoWhiteBalanceSettings(s);
    RendererSettings rs;
    rs.enableAutoWhiteBalance = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.autoWhiteBalanceActive);
}
