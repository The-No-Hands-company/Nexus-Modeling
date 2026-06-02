// Null-backend tests for Spectral Participating Media (v0.21 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct SpectralMediaFixture : public ::testing::Test {
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

TEST_F(SpectralMediaFixture, SpectralMediaSettingsDefaultDisabled) {
    SpectralMediaSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(SpectralMediaFixture, SpectralMediaSettingsDefaultSpectralBands) {
    SpectralMediaSettings s;
    EXPECT_EQ(s.spectralBands, 8u);
}

TEST_F(SpectralMediaFixture, SpectralMediaSettingsDefaultExtinctionScale) {
    SpectralMediaSettings s;
    EXPECT_FLOAT_EQ(s.extinctionScale, 1.f);
}

TEST_F(SpectralMediaFixture, SpectralMediaSettingsDefaultScatteringScale) {
    SpectralMediaSettings s;
    EXPECT_FLOAT_EQ(s.scatteringScale, 1.f);
}

TEST_F(SpectralMediaFixture, SetAndGetSpectralMediaSettings) {
    Renderer r(*ctx, *sc);
    SpectralMediaSettings s;
    s.enabled         = true;
    s.spectralBands   = 16;
    s.extinctionScale = 2.f;
    s.scatteringScale = 0.5f;
    r.setSpectralMediaSettings(s);
    EXPECT_TRUE     (r.spectralMediaSettings().enabled);
    EXPECT_EQ       (r.spectralMediaSettings().spectralBands,   16u);
    EXPECT_FLOAT_EQ (r.spectralMediaSettings().extinctionScale, 2.f);
    EXPECT_FLOAT_EQ (r.spectralMediaSettings().scatteringScale, 0.5f);
}

TEST_F(SpectralMediaFixture, SpectralMediaGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    SpectralMediaSettings s;
    s.enabled       = true;
    s.spectralBands = 8;
    r.setSpectralMediaSettings(s);
    RendererSettings rs;
    rs.enableSpectralMedia = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.spectralMediaActive);
    EXPECT_EQ   (stats.spectralMediaBandCount, 0u);
}

TEST_F(SpectralMediaFixture, SpectralMediaGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    SpectralMediaSettings s;
    s.enabled       = true;
    s.spectralBands = 8;
    r.setSpectralMediaSettings(s);
    RendererSettings rs;
    rs.enableSpectralMedia = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.spectralMediaActive);
    EXPECT_EQ  (stats.spectralMediaBandCount, 8u);
}

TEST_F(SpectralMediaFixture, SpectralMediaBandCountReflectedInStat) {
    Renderer r(*ctx, *sc);
    SpectralMediaSettings s;
    s.enabled       = true;
    s.spectralBands = 24;
    r.setSpectralMediaSettings(s);
    RendererSettings rs;
    rs.enableSpectralMedia = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.spectralMediaBandCount, 24u);
}

TEST_F(SpectralMediaFixture, SpectralMediaInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    SpectralMediaSettings s;
    s.enabled       = false;
    s.spectralBands = 8;
    r.setSpectralMediaSettings(s);
    RendererSettings rs;
    rs.enableSpectralMedia = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.spectralMediaActive);
}
