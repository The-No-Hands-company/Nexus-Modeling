// Null-backend tests for Multi-Spectral Rendering (v0.18 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct MultiSpectralFixture : public ::testing::Test {
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

TEST_F(MultiSpectralFixture, MultiSpectralSettingsDefaultDisabled) {
    MultiSpectralSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(MultiSpectralFixture, MultiSpectralSettingsDefaultWavelengthBands) {
    MultiSpectralSettings s;
    EXPECT_EQ(s.wavelengthBands, 8u);
}

TEST_F(MultiSpectralFixture, MultiSpectralSettingsDefaultWavelengthRange) {
    MultiSpectralSettings s;
    EXPECT_FLOAT_EQ(s.minWavelengthNm, 380.f);
    EXPECT_FLOAT_EQ(s.maxWavelengthNm, 780.f);
}

TEST_F(MultiSpectralFixture, MultiSpectralSettingsDefaultSamplesPerBand) {
    MultiSpectralSettings s;
    EXPECT_EQ(s.samplesPerBand, 2u);
}

TEST_F(MultiSpectralFixture, SetAndGetMultiSpectralSettings) {
    Renderer r(*ctx, *sc);
    MultiSpectralSettings s;
    s.enabled         = true;
    s.wavelengthBands = 16;
    s.minWavelengthNm = 400.f;
    s.maxWavelengthNm = 700.f;
    s.samplesPerBand  = 4;
    r.setMultiSpectralSettings(s);
    EXPECT_TRUE     (r.multiSpectralSettings().enabled);
    EXPECT_EQ       (r.multiSpectralSettings().wavelengthBands, 16u);
    EXPECT_FLOAT_EQ (r.multiSpectralSettings().minWavelengthNm, 400.f);
    EXPECT_FLOAT_EQ (r.multiSpectralSettings().maxWavelengthNm, 700.f);
    EXPECT_EQ       (r.multiSpectralSettings().samplesPerBand,  4u);
}

TEST_F(MultiSpectralFixture, MultiSpectralGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    MultiSpectralSettings s;
    s.enabled         = true;
    s.wavelengthBands = 8;
    r.setMultiSpectralSettings(s);
    RendererSettings rs;
    rs.enableMultiSpectral = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.multiSpectralActive);
    EXPECT_EQ   (stats.multiSpectralBandCount, 0u);
}

TEST_F(MultiSpectralFixture, MultiSpectralGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    MultiSpectralSettings s;
    s.enabled         = true;
    s.wavelengthBands = 8;
    r.setMultiSpectralSettings(s);
    RendererSettings rs;
    rs.enableMultiSpectral = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.multiSpectralActive);
    EXPECT_EQ  (stats.multiSpectralBandCount, 8u);
}

TEST_F(MultiSpectralFixture, MultiSpectralBandCountReflectedInStat) {
    Renderer r(*ctx, *sc);
    MultiSpectralSettings s;
    s.enabled         = true;
    s.wavelengthBands = 32;
    r.setMultiSpectralSettings(s);
    RendererSettings rs;
    rs.enableMultiSpectral = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.multiSpectralBandCount, 32u);
}

TEST_F(MultiSpectralFixture, MultiSpectralInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    MultiSpectralSettings s;
    s.enabled         = false;
    s.wavelengthBands = 8;
    r.setMultiSpectralSettings(s);
    RendererSettings rs;
    rs.enableMultiSpectral = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.multiSpectralActive);
}
