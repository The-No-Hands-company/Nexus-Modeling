// Null-backend tests for Spectral Upsampling from RGB Assets (v0.19 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct SpectralUpsamplingFixture : public ::testing::Test {
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

TEST_F(SpectralUpsamplingFixture, SpectralUpsamplingSettingsDefaultDisabled) {
    SpectralUpsamplingSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(SpectralUpsamplingFixture, SpectralUpsamplingSettingsDefaultMethod) {
    SpectralUpsamplingSettings s;
    EXPECT_EQ(s.method, SpectralUpsamplingMethod::Smits);
}

TEST_F(SpectralUpsamplingFixture, SpectralUpsamplingSettingsDefaultBands) {
    SpectralUpsamplingSettings s;
    EXPECT_EQ(s.upsamplingBands, 8u);
}

TEST_F(SpectralUpsamplingFixture, SetAndGetSpectralUpsamplingSettings) {
    Renderer r(*ctx, *sc);
    SpectralUpsamplingSettings s;
    s.enabled        = true;
    s.method         = SpectralUpsamplingMethod::Sigmoid;
    s.upsamplingBands = 16;
    r.setSpectralUpsamplingSettings(s);
    EXPECT_TRUE (r.spectralUpsamplingSettings().enabled);
    EXPECT_EQ   (r.spectralUpsamplingSettings().method,          SpectralUpsamplingMethod::Sigmoid);
    EXPECT_EQ   (r.spectralUpsamplingSettings().upsamplingBands, 16u);
}

TEST_F(SpectralUpsamplingFixture, SpectralUpsamplingGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    SpectralUpsamplingSettings s;
    s.enabled = true;
    r.setSpectralUpsamplingSettings(s);
    RendererSettings rs;
    rs.enableSpectralUpsampling = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.spectralUpsamplingActive);
}

TEST_F(SpectralUpsamplingFixture, SpectralUpsamplingGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    SpectralUpsamplingSettings s;
    s.enabled = true;
    r.setSpectralUpsamplingSettings(s);
    RendererSettings rs;
    rs.enableSpectralUpsampling = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.spectralUpsamplingActive);
}

TEST_F(SpectralUpsamplingFixture, SpectralUpsamplingMethodReflected_Smits) {
    Renderer r(*ctx, *sc);
    SpectralUpsamplingSettings s;
    s.enabled = true;
    s.method  = SpectralUpsamplingMethod::Smits;
    r.setSpectralUpsamplingSettings(s);
    RendererSettings rs;
    rs.enableSpectralUpsampling = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.spectralUpsamplingMethod, SpectralUpsamplingMethod::Smits);
}

TEST_F(SpectralUpsamplingFixture, SpectralUpsamplingInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    SpectralUpsamplingSettings s;
    s.enabled = false;
    r.setSpectralUpsamplingSettings(s);
    RendererSettings rs;
    rs.enableSpectralUpsampling = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.spectralUpsamplingActive);
}
