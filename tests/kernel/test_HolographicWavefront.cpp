// Null-backend tests for Holographic Wavefront Encoding (v0.23 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct HolographicWavefrontFixture : public ::testing::Test {
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

TEST_F(HolographicWavefrontFixture, HolographicWavefrontSettingsDefaultDisabled) {
    HolographicWavefrontSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(HolographicWavefrontFixture, HolographicWavefrontSettingsDefaultDepthSlices) {
    HolographicWavefrontSettings s;
    EXPECT_EQ(s.depthSlices, 8u);
}

TEST_F(HolographicWavefrontFixture, HolographicWavefrontSettingsDefaultPupilDiameter) {
    HolographicWavefrontSettings s;
    EXPECT_FLOAT_EQ(s.pupilDiameterMm, 4.f);
}

TEST_F(HolographicWavefrontFixture, HolographicWavefrontSettingsDefaultWavelength) {
    HolographicWavefrontSettings s;
    EXPECT_FLOAT_EQ(s.wavelengthNm, 532.f);
}

TEST_F(HolographicWavefrontFixture, SetAndGetHolographicWavefrontSettings) {
    Renderer r(*ctx, *sc);
    HolographicWavefrontSettings s;
    s.enabled         = true;
    s.depthSlices     = 16;
    s.pupilDiameterMm = 3.f;
    s.wavelengthNm    = 633.f;
    r.setHolographicWavefrontSettings(s);
    EXPECT_TRUE     (r.holographicWavefrontSettings().enabled);
    EXPECT_EQ       (r.holographicWavefrontSettings().depthSlices,     16u);
    EXPECT_FLOAT_EQ (r.holographicWavefrontSettings().pupilDiameterMm, 3.f);
    EXPECT_FLOAT_EQ (r.holographicWavefrontSettings().wavelengthNm,    633.f);
}

TEST_F(HolographicWavefrontFixture, HolographicWavefrontGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    HolographicWavefrontSettings s;
    s.enabled     = true;
    s.depthSlices = 8;
    r.setHolographicWavefrontSettings(s);
    RendererSettings rs;
    rs.enableHolographicWavefront = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.holographicWavefrontActive);
    EXPECT_EQ   (stats.holographicWavefrontSliceCount, 0u);
}

TEST_F(HolographicWavefrontFixture, HolographicWavefrontGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    HolographicWavefrontSettings s;
    s.enabled     = true;
    s.depthSlices = 8;
    r.setHolographicWavefrontSettings(s);
    RendererSettings rs;
    rs.enableHolographicWavefront = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.holographicWavefrontActive);
    EXPECT_EQ  (stats.holographicWavefrontSliceCount, 8u);
}

TEST_F(HolographicWavefrontFixture, HolographicWavefrontSliceCountReflectedInStat) {
    Renderer r(*ctx, *sc);
    HolographicWavefrontSettings s;
    s.enabled     = true;
    s.depthSlices = 24;
    r.setHolographicWavefrontSettings(s);
    RendererSettings rs;
    rs.enableHolographicWavefront = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.holographicWavefrontSliceCount, 24u);
}

TEST_F(HolographicWavefrontFixture, HolographicWavefrontInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    HolographicWavefrontSettings s;
    s.enabled     = false;
    s.depthSlices = 8;
    r.setHolographicWavefrontSettings(s);
    RendererSettings rs;
    rs.enableHolographicWavefront = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.holographicWavefrontActive);
}
