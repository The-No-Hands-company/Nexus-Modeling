// Null-backend tests for Coherent Wave Optics (v0.21 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct WaveOpticsFixture : public ::testing::Test {
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

TEST_F(WaveOpticsFixture, WaveOpticsSettingsDefaultDisabled) {
    WaveOpticsSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(WaveOpticsFixture, WaveOpticsSettingsDefaultDiffraction) {
    WaveOpticsSettings s;
    EXPECT_TRUE(s.diffractionEnabled);
}

TEST_F(WaveOpticsFixture, WaveOpticsSettingsDefaultInterference) {
    WaveOpticsSettings s;
    EXPECT_TRUE(s.interferenceEnabled);
}

TEST_F(WaveOpticsFixture, WaveOpticsSettingsDefaultApertureSize) {
    WaveOpticsSettings s;
    EXPECT_FLOAT_EQ(s.apertureSize, 0.01f);
}

TEST_F(WaveOpticsFixture, SetAndGetWaveOpticsSettings) {
    Renderer r(*ctx, *sc);
    WaveOpticsSettings s;
    s.enabled              = true;
    s.diffractionEnabled   = false;
    s.interferenceEnabled  = true;
    s.apertureSize         = 0.02f;
    r.setWaveOpticsSettings(s);
    EXPECT_TRUE     (r.waveOpticsSettings().enabled);
    EXPECT_FALSE    (r.waveOpticsSettings().diffractionEnabled);
    EXPECT_TRUE     (r.waveOpticsSettings().interferenceEnabled);
    EXPECT_FLOAT_EQ (r.waveOpticsSettings().apertureSize, 0.02f);
}

TEST_F(WaveOpticsFixture, WaveOpticsGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    WaveOpticsSettings s;
    s.enabled = true;
    r.setWaveOpticsSettings(s);
    RendererSettings rs;
    rs.enableWaveOptics = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.waveOpticsActive);
    EXPECT_EQ   (stats.waveOpticsPassCount, 0u);
}

TEST_F(WaveOpticsFixture, WaveOpticsGateTrue_BothEffects_PassCount5) {
    Renderer r(*ctx, *sc);
    WaveOpticsSettings s;
    s.enabled             = true;
    s.diffractionEnabled  = true;
    s.interferenceEnabled = true;
    r.setWaveOpticsSettings(s);
    RendererSettings rs;
    rs.enableWaveOptics = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.waveOpticsActive);
    EXPECT_EQ  (stats.waveOpticsPassCount, 5u); // 3 diffraction + 2 interference
}

TEST_F(WaveOpticsFixture, WaveOpticsDiffractionOnly_PassCount3) {
    Renderer r(*ctx, *sc);
    WaveOpticsSettings s;
    s.enabled             = true;
    s.diffractionEnabled  = true;
    s.interferenceEnabled = false;
    r.setWaveOpticsSettings(s);
    RendererSettings rs;
    rs.enableWaveOptics = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.waveOpticsPassCount, 3u);
}

TEST_F(WaveOpticsFixture, WaveOpticsInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    WaveOpticsSettings s;
    s.enabled = false;
    r.setWaveOpticsSettings(s);
    RendererSettings rs;
    rs.enableWaveOptics = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.waveOpticsActive);
}
