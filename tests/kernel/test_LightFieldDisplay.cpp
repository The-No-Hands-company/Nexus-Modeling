// Null-backend tests for Light-Field Display Output (v0.22 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct LightFieldDisplayFixture : public ::testing::Test {
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

TEST_F(LightFieldDisplayFixture, LightFieldDisplaySettingsDefaultDisabled) {
    LightFieldDisplaySettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(LightFieldDisplayFixture, LightFieldDisplaySettingsDefaultViewColumns) {
    LightFieldDisplaySettings s;
    EXPECT_EQ(s.viewColumns, 8u);
}

TEST_F(LightFieldDisplayFixture, LightFieldDisplaySettingsDefaultViewRows) {
    LightFieldDisplaySettings s;
    EXPECT_EQ(s.viewRows, 4u);
}

TEST_F(LightFieldDisplayFixture, LightFieldDisplaySettingsDefaultType) {
    LightFieldDisplaySettings s;
    EXPECT_EQ(s.displayType, LightFieldDisplayType::Lenticular);
}

TEST_F(LightFieldDisplayFixture, SetAndGetLightFieldDisplaySettings) {
    Renderer r(*ctx, *sc);
    LightFieldDisplaySettings s;
    s.enabled     = true;
    s.viewColumns = 10;
    s.viewRows    = 6;
    s.displayType = LightFieldDisplayType::Holographic;
    r.setLightFieldDisplaySettings(s);
    EXPECT_TRUE(r.lightFieldDisplaySettings().enabled);
    EXPECT_EQ  (r.lightFieldDisplaySettings().viewColumns, 10u);
    EXPECT_EQ  (r.lightFieldDisplaySettings().viewRows,    6u);
    EXPECT_EQ  (r.lightFieldDisplaySettings().displayType, LightFieldDisplayType::Holographic);
}

TEST_F(LightFieldDisplayFixture, LightFieldDisplayGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    LightFieldDisplaySettings s;
    s.enabled     = true;
    s.viewColumns = 8;
    s.viewRows    = 4;
    r.setLightFieldDisplaySettings(s);
    RendererSettings rs;
    rs.enableLightFieldDisplay = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.lightFieldDisplayActive);
    EXPECT_EQ   (stats.lightFieldDisplayViewCount, 0u);
}

TEST_F(LightFieldDisplayFixture, LightFieldDisplayGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    LightFieldDisplaySettings s;
    s.enabled     = true;
    s.viewColumns = 8;
    s.viewRows    = 4;
    r.setLightFieldDisplaySettings(s);
    RendererSettings rs;
    rs.enableLightFieldDisplay = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.lightFieldDisplayActive);
    EXPECT_EQ  (stats.lightFieldDisplayViewCount, 32u);
}

TEST_F(LightFieldDisplayFixture, LightFieldDisplayViewCountReflectsColsRows) {
    Renderer r(*ctx, *sc);
    LightFieldDisplaySettings s;
    s.enabled     = true;
    s.viewColumns = 5;
    s.viewRows    = 3;
    r.setLightFieldDisplaySettings(s);
    RendererSettings rs;
    rs.enableLightFieldDisplay = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.lightFieldDisplayViewCount, 15u);
}

TEST_F(LightFieldDisplayFixture, LightFieldDisplayInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    LightFieldDisplaySettings s;
    s.enabled     = false;
    s.viewColumns = 8;
    s.viewRows    = 4;
    r.setLightFieldDisplaySettings(s);
    RendererSettings rs;
    rs.enableLightFieldDisplay = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.lightFieldDisplayActive);
}
