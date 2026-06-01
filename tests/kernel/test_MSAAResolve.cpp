// ─────────────────────────────────────────────────────────────────────────────
//  Tests: MSAA Resolve (v0.7 Track 3)
//
//  Verifies RendererSettings::msaaSamples, DeviceCapabilities::maxMsaaSamples,
//  FrameStats::msaaSamples clamping, and ICommandBuffer::resolveTexture.
//  All tests run on Null backend where maxMsaaSamples == 1.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct MSAAFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain> sc;
    IDevice* dev = nullptr;

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
        dev = &ctx->device();
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

// ── DeviceCapabilities ────────────────────────────────────────────────────────

TEST_F(MSAAFixture, NullBackendReportsMaxMsaaSamplesOfOne) {
    EXPECT_EQ(dev->caps().maxMsaaSamples, 1u);
}

// ── Settings ──────────────────────────────────────────────────────────────────

TEST_F(MSAAFixture, DefaultMsaaSamplesIsOne) {
    Renderer renderer(*ctx, *sc);
    EXPECT_EQ(renderer.settings().msaaSamples, 1u);
}

TEST_F(MSAAFixture, MsaaSamplesSettingRoundTrip) {
    Renderer renderer(*ctx, *sc);
    RendererSettings s = renderer.settings();
    s.msaaSamples = 4;
    renderer.applySettings(s);
    EXPECT_EQ(renderer.settings().msaaSamples, 4u);
}

// ── FrameStats clamping ───────────────────────────────────────────────────────

TEST_F(MSAAFixture, MsaaSamplesStatIsOneWhenSettingIsOne) {
    Renderer renderer(*ctx, *sc);
    FrameStats stats = renderOnce(renderer);
    EXPECT_EQ(stats.msaaSamples, 1u);
}

TEST_F(MSAAFixture, MsaaSamplesStatClampedToMaxCaps) {
    // Request 4× MSAA on Null backend (max == 1) — stat must be clamped to 1.
    Renderer renderer(*ctx, *sc);
    RendererSettings s = renderer.settings();
    s.msaaSamples = 4;
    renderer.applySettings(s);

    FrameStats stats = renderOnce(renderer);
    EXPECT_EQ(stats.msaaSamples,
              std::min(uint8_t{4}, dev->caps().maxMsaaSamples));
    EXPECT_EQ(stats.msaaSamples, 1u);  // Null clamp
}

TEST_F(MSAAFixture, MsaaSamplesStatNeverExceedsMaxCaps) {
    Renderer renderer(*ctx, *sc);
    RendererSettings s = renderer.settings();
    s.msaaSamples = 8;
    renderer.applySettings(s);

    FrameStats stats = renderOnce(renderer);
    EXPECT_LE(stats.msaaSamples, dev->caps().maxMsaaSamples);
}

// ── resolveTexture no-op on Null ──────────────────────────────────────────────

TEST_F(MSAAFixture, ResolveTextureWithInvalidHandlesDoesNotCrash) {
    // Null backend resolveTexture is a no-op — calling it with invalid handles
    // must not crash or assert.
    CmdBufHandle handle = dev->allocateCommandBuffer(QueueType::Graphics);
    ICommandBuffer* cmd = dev->getCommandBuffer(handle);
    ASSERT_NE(cmd, nullptr);
    EXPECT_NO_THROW(cmd->resolveTexture({}, {}));
    dev->freeCommandBuffer(handle);
}

}  // namespace
