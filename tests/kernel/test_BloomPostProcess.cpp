// Null-backend tests for the Bloom compute chain (v0.8 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct BloomFixture : public ::testing::Test {
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

TEST_F(BloomFixture, BloomSettingsDefaultValues) {
    BloomSettings bloom;
    EXPECT_FLOAT_EQ(bloom.threshold, 1.f);
    EXPECT_FLOAT_EQ(bloom.intensity, 0.04f);
    EXPECT_FLOAT_EQ(bloom.radius,    0.85f);
    EXPECT_EQ      (bloom.passes,    5u);
}

TEST_F(BloomFixture, SetAndGetBloomSettings) {
    Renderer r(*ctx, *sc);
    BloomSettings bloom;
    bloom.threshold = 0.8f;
    bloom.intensity = 0.1f;
    bloom.radius    = 0.5f;
    bloom.passes    = 3;
    r.setBloomSettings(bloom);
    EXPECT_FLOAT_EQ(r.bloomSettings().threshold, 0.8f);
    EXPECT_FLOAT_EQ(r.bloomSettings().intensity, 0.1f);
    EXPECT_FLOAT_EQ(r.bloomSettings().radius,    0.5f);
    EXPECT_EQ      (r.bloomSettings().passes,    3u);
}

TEST_F(BloomFixture, EnableBloomFlagDefaultTrue) {
    RendererSettings settings;
    EXPECT_TRUE(settings.enableBloom);
}

TEST_F(BloomFixture, BloomActiveWhenEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableBloom = true;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.bloomActive);
}

TEST_F(BloomFixture, BloomInactiveWhenDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableBloom = false;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.bloomActive);
}

TEST_F(BloomFixture, BloomPassCountIsDoublePassSetting) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableBloom = true;
    r.applySettings(s);
    BloomSettings bloom;
    bloom.passes = 3;
    r.setBloomSettings(bloom);
    FrameStats stats = renderOnce(r);
    // Each pass = 1 downsample + 1 upsample dispatch
    EXPECT_EQ(stats.bloomPassCount, 6u);
}

TEST_F(BloomFixture, BloomPassCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableBloom = false;
    r.applySettings(s);
    FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.bloomPassCount, 0u);
}

TEST_F(BloomFixture, BloomSettingsRoundTripIndependent) {
    Renderer r(*ctx, *sc);
    BloomSettings a;
    a.threshold = 0.5f;
    a.passes    = 2;
    r.setBloomSettings(a);
    BloomSettings b;
    b.threshold = 1.5f;
    b.passes    = 7;
    r.setBloomSettings(b);
    EXPECT_FLOAT_EQ(r.bloomSettings().threshold, 1.5f);
    EXPECT_EQ      (r.bloomSettings().passes,    7u);
}
