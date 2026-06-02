// Null-backend tests for Hyperspectral IBL (v0.20 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct HyperspectralIBLFixture : public ::testing::Test {
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

TEST_F(HyperspectralIBLFixture, HyperspectralIBLSettingsDefaultDisabled) {
    HyperspectralIBLSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(HyperspectralIBLFixture, HyperspectralIBLSettingsDefaultSpectralBands) {
    HyperspectralIBLSettings s;
    EXPECT_EQ(s.spectralBands, 8u);
}

TEST_F(HyperspectralIBLFixture, HyperspectralIBLSettingsDefaultMipLevels) {
    HyperspectralIBLSettings s;
    EXPECT_EQ(s.envMapMipLevels, 6u);
}

TEST_F(HyperspectralIBLFixture, HyperspectralIBLSettingsDefaultImportanceSamples) {
    HyperspectralIBLSettings s;
    EXPECT_EQ(s.importanceSampleCount, 64u);
}

TEST_F(HyperspectralIBLFixture, SetAndGetHyperspectralIBLSettings) {
    Renderer r(*ctx, *sc);
    HyperspectralIBLSettings s;
    s.enabled               = true;
    s.spectralBands         = 16;
    s.envMapMipLevels       = 8;
    s.importanceSampleCount = 128;
    r.setHyperspectralIBLSettings(s);
    EXPECT_TRUE(r.hyperspectralIBLSettings().enabled);
    EXPECT_EQ  (r.hyperspectralIBLSettings().spectralBands,         16u);
    EXPECT_EQ  (r.hyperspectralIBLSettings().envMapMipLevels,        8u);
    EXPECT_EQ  (r.hyperspectralIBLSettings().importanceSampleCount, 128u);
}

TEST_F(HyperspectralIBLFixture, HyperspectralIBLGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    HyperspectralIBLSettings s;
    s.enabled       = true;
    s.spectralBands = 8;
    r.setHyperspectralIBLSettings(s);
    RendererSettings rs;
    rs.enableHyperspectralIBL = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.hyperspectralIBLActive);
    EXPECT_EQ   (stats.hyperspectralIBLBandCount, 0u);
}

TEST_F(HyperspectralIBLFixture, HyperspectralIBLGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    HyperspectralIBLSettings s;
    s.enabled       = true;
    s.spectralBands = 8;
    r.setHyperspectralIBLSettings(s);
    RendererSettings rs;
    rs.enableHyperspectralIBL = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.hyperspectralIBLActive);
    EXPECT_EQ  (stats.hyperspectralIBLBandCount, 8u);
}

TEST_F(HyperspectralIBLFixture, HyperspectralIBLBandCountReflectedInStat) {
    Renderer r(*ctx, *sc);
    HyperspectralIBLSettings s;
    s.enabled       = true;
    s.spectralBands = 32;
    r.setHyperspectralIBLSettings(s);
    RendererSettings rs;
    rs.enableHyperspectralIBL = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.hyperspectralIBLBandCount, 32u);
}

TEST_F(HyperspectralIBLFixture, HyperspectralIBLInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    HyperspectralIBLSettings s;
    s.enabled       = false;
    s.spectralBands = 8;
    r.setHyperspectralIBLSettings(s);
    RendererSettings rs;
    rs.enableHyperspectralIBL = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.hyperspectralIBLActive);
}
