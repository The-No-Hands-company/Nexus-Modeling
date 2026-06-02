// Null-backend tests for Gaussian Splatting Spectral Extensions (v0.22 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct GaussianSplatSpectralFixture : public ::testing::Test {
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

TEST_F(GaussianSplatSpectralFixture, GaussianSplatSpectralSettingsDefaultDisabled) {
    GaussianSplatSpectralSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(GaussianSplatSpectralFixture, GaussianSplatSpectralSettingsDefaultSpectralCoefficients) {
    GaussianSplatSpectralSettings s;
    EXPECT_EQ(s.spectralCoefficients, 9u);
}

TEST_F(GaussianSplatSpectralFixture, GaussianSplatSpectralSettingsDefaultSpectralBands) {
    GaussianSplatSpectralSettings s;
    EXPECT_EQ(s.spectralBands, 8u);
}

TEST_F(GaussianSplatSpectralFixture, SetAndGetGaussianSplatSpectralSettings) {
    Renderer r(*ctx, *sc);
    GaussianSplatSpectralSettings s;
    s.enabled              = true;
    s.spectralCoefficients = 16;
    s.spectralBands        = 12;
    r.setGaussianSplatSpectralSettings(s);
    EXPECT_TRUE(r.gaussianSplatSpectralSettings().enabled);
    EXPECT_EQ  (r.gaussianSplatSpectralSettings().spectralCoefficients, 16u);
    EXPECT_EQ  (r.gaussianSplatSpectralSettings().spectralBands,        12u);
}

TEST_F(GaussianSplatSpectralFixture, GaussianSplatSpectralGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    GaussianSplatSpectralSettings s;
    s.enabled       = true;
    s.spectralBands = 8;
    r.setGaussianSplatSpectralSettings(s);
    RendererSettings rs;
    rs.enableGaussianSplatSpectral = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.gaussianSplatSpectralActive);
    EXPECT_EQ   (stats.gaussianSplatSpectralBandCount, 0u);
}

TEST_F(GaussianSplatSpectralFixture, GaussianSplatSpectralGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    GaussianSplatSpectralSettings s;
    s.enabled       = true;
    s.spectralBands = 8;
    r.setGaussianSplatSpectralSettings(s);
    RendererSettings rs;
    rs.enableGaussianSplatSpectral = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.gaussianSplatSpectralActive);
    EXPECT_EQ  (stats.gaussianSplatSpectralBandCount, 8u);
}

TEST_F(GaussianSplatSpectralFixture, GaussianSplatSpectralBandCountReflectedInStat) {
    Renderer r(*ctx, *sc);
    GaussianSplatSpectralSettings s;
    s.enabled       = true;
    s.spectralBands = 16;
    r.setGaussianSplatSpectralSettings(s);
    RendererSettings rs;
    rs.enableGaussianSplatSpectral = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.gaussianSplatSpectralBandCount, 16u);
}

TEST_F(GaussianSplatSpectralFixture, GaussianSplatSpectralInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    GaussianSplatSpectralSettings s;
    s.enabled       = false;
    s.spectralBands = 8;
    r.setGaussianSplatSpectralSettings(s);
    RendererSettings rs;
    rs.enableGaussianSplatSpectral = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.gaussianSplatSpectralActive);
}
