// Null-backend tests for Plenoptic / Light-Field Rendering (v0.21 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct PlenopticFixture : public ::testing::Test {
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

TEST_F(PlenopticFixture, PlenopticSettingsDefaultDisabled) {
    PlenopticSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(PlenopticFixture, PlenopticSettingsDefaultAngularResolution) {
    PlenopticSettings s;
    EXPECT_EQ(s.angularResolution, 8u);
}

TEST_F(PlenopticFixture, PlenopticSettingsDefaultSpatialResolution) {
    PlenopticSettings s;
    EXPECT_EQ(s.spatialResolution, 512u);
}

TEST_F(PlenopticFixture, PlenopticSettingsDefaultRefocusDepth) {
    PlenopticSettings s;
    EXPECT_FLOAT_EQ(s.refocusDepth, 1.f);
}

TEST_F(PlenopticFixture, SetAndGetPlenopticSettings) {
    Renderer r(*ctx, *sc);
    PlenopticSettings s;
    s.enabled           = true;
    s.angularResolution = 16;
    s.spatialResolution = 1024;
    s.refocusDepth      = 5.f;
    r.setPlenopticSettings(s);
    EXPECT_TRUE     (r.plenopticSettings().enabled);
    EXPECT_EQ       (r.plenopticSettings().angularResolution, 16u);
    EXPECT_EQ       (r.plenopticSettings().spatialResolution, 1024u);
    EXPECT_FLOAT_EQ (r.plenopticSettings().refocusDepth,      5.f);
}

TEST_F(PlenopticFixture, PlenopticGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    PlenopticSettings s;
    s.enabled           = true;
    s.angularResolution = 8;
    r.setPlenopticSettings(s);
    RendererSettings rs;
    rs.enablePlenoptic = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.plenopticActive);
    EXPECT_EQ   (stats.plenopticRayCount, 0u);
}

TEST_F(PlenopticFixture, PlenopticGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    PlenopticSettings s;
    s.enabled           = true;
    s.angularResolution = 8;
    r.setPlenopticSettings(s);
    RendererSettings rs;
    rs.enablePlenoptic = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.plenopticActive);
    EXPECT_GT  (stats.plenopticRayCount, 0u);
}

TEST_F(PlenopticFixture, PlenopticRayCountReflectsAngularResolution) {
    Renderer r(*ctx, *sc);
    PlenopticSettings s;
    s.enabled           = true;
    s.angularResolution = 4;
    r.setPlenopticSettings(s);
    RendererSettings rs;
    rs.enablePlenoptic = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    // rayCount = width × height × ar × ar
    const uint32_t expected = 640u * 480u * 4u * 4u;
    EXPECT_EQ(stats.plenopticRayCount, expected);
}

TEST_F(PlenopticFixture, PlenopticInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    PlenopticSettings s;
    s.enabled           = false;
    s.angularResolution = 8;
    r.setPlenopticSettings(s);
    RendererSettings rs;
    rs.enablePlenoptic = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.plenopticActive);
}
