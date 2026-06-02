// Null-backend tests for Polarisation Rendering (v0.19 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct PolarisationFixture : public ::testing::Test {
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

TEST_F(PolarisationFixture, PolarisationSettingsDefaultDisabled) {
    PolarisationSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(PolarisationFixture, PolarisationSettingsDefaultStokesComponents) {
    PolarisationSettings s;
    EXPECT_EQ(s.stokesComponents, 4u);
}

TEST_F(PolarisationFixture, PolarisationSettingsDefaultTrackCircular) {
    PolarisationSettings s;
    EXPECT_TRUE(s.trackCircular);
}

TEST_F(PolarisationFixture, SetAndGetPolarisationSettings) {
    Renderer r(*ctx, *sc);
    PolarisationSettings s;
    s.enabled          = true;
    s.stokesComponents = 3;
    s.trackCircular    = false;
    r.setPolarisationSettings(s);
    EXPECT_TRUE (r.polarisationSettings().enabled);
    EXPECT_EQ   (r.polarisationSettings().stokesComponents, 3u);
    EXPECT_FALSE(r.polarisationSettings().trackCircular);
}

TEST_F(PolarisationFixture, PolarisationGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    PolarisationSettings s;
    s.enabled          = true;
    s.stokesComponents = 4;
    r.setPolarisationSettings(s);
    RendererSettings rs;
    rs.enablePolarisation = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.polarisationActive);
    EXPECT_EQ   (stats.polarisationRayCount, 0u);
}

TEST_F(PolarisationFixture, PolarisationGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    PolarisationSettings s;
    s.enabled          = true;
    s.stokesComponents = 4;
    r.setPolarisationSettings(s);
    RendererSettings rs;
    rs.enablePolarisation = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.polarisationActive);
    EXPECT_GT  (stats.polarisationRayCount, 0u);
}

TEST_F(PolarisationFixture, PolarisationRayCountReflectsComponents) {
    Renderer r(*ctx, *sc);
    PolarisationSettings s;
    s.enabled          = true;
    s.stokesComponents = 2;
    r.setPolarisationSettings(s);
    RendererSettings rs;
    rs.enablePolarisation = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    // rayCount = width × height × components
    const uint32_t expected = 640u * 480u * 2u;
    EXPECT_EQ(stats.polarisationRayCount, expected);
}

TEST_F(PolarisationFixture, PolarisationInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    PolarisationSettings s;
    s.enabled          = false;
    s.stokesComponents = 4;
    r.setPolarisationSettings(s);
    RendererSettings rs;
    rs.enablePolarisation = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.polarisationActive);
}
