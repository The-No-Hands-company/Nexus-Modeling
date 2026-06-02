// Null-backend tests for Hero Wavelength Spectral Dispersion (v0.17 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct SpectralFixture : public ::testing::Test {
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

TEST_F(SpectralFixture, SpectralSettingsDefaultDisabled) {
    SpectralSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(SpectralFixture, SpectralSettingsDefaultWavelengthSamples) {
    SpectralSettings s;
    EXPECT_EQ(s.wavelengthSamples, 4u);
}

TEST_F(SpectralFixture, SpectralSettingsDefaultDispersionScale) {
    SpectralSettings s;
    EXPECT_FLOAT_EQ(s.dispersionScale, 1.f);
}

TEST_F(SpectralFixture, SpectralSettingsDefaultHeroWavelength) {
    SpectralSettings s;
    EXPECT_FLOAT_EQ(s.heroWavelengthNm, 550.f);
}

TEST_F(SpectralFixture, SetAndGetSpectralSettings) {
    Renderer r(*ctx, *sc);
    SpectralSettings s;
    s.enabled           = true;
    s.wavelengthSamples = 8;
    s.dispersionScale   = 2.f;
    s.heroWavelengthNm  = 480.f;
    r.setSpectralSettings(s);
    EXPECT_TRUE     (r.spectralSettings().enabled);
    EXPECT_EQ       (r.spectralSettings().wavelengthSamples, 8u);
    EXPECT_FLOAT_EQ (r.spectralSettings().dispersionScale,   2.f);
    EXPECT_FLOAT_EQ (r.spectralSettings().heroWavelengthNm,  480.f);
}

TEST_F(SpectralFixture, SpectralGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    SpectralSettings s;
    s.enabled           = true;
    s.wavelengthSamples = 4;
    r.setSpectralSettings(s);
    RendererSettings rs;
    rs.enableSpectral = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.spectralActive);
    EXPECT_EQ   (stats.spectralWavelengthSamples, 0u);
}

TEST_F(SpectralFixture, SpectralGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    SpectralSettings s;
    s.enabled           = true;
    s.wavelengthSamples = 4;
    r.setSpectralSettings(s);
    RendererSettings rs;
    rs.enableSpectral = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.spectralActive);
    EXPECT_EQ  (stats.spectralWavelengthSamples, 4u);
}

TEST_F(SpectralFixture, SpectralWavelengthSamplesReflectedInStat) {
    Renderer r(*ctx, *sc);
    SpectralSettings s;
    s.enabled           = true;
    s.wavelengthSamples = 16;
    r.setSpectralSettings(s);
    RendererSettings rs;
    rs.enableSpectral = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.spectralWavelengthSamples, 16u);
}

TEST_F(SpectralFixture, SpectralInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    SpectralSettings s;
    s.enabled           = false;
    s.wavelengthSamples = 4;
    r.setSpectralSettings(s);
    RendererSettings rs;
    rs.enableSpectral = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.spectralActive);
}
