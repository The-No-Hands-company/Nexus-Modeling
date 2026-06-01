// ─────────────────────────────────────────────────────────────────────────────
//  Tests: MSAA Resolve (v0.7 Track 3)
//
//  Verifies RendererSettings::msaaSamples, DeviceCapabilities::maxMsaaSamples,
//  FrameStats::msaaSamples clamping, and ICommandBuffer::resolveTexture.
//  All tests run on Null backend where maxMsaaSamples == 1.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct MSAAFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    IDevice* dev = nullptr;

    void SetUp() override {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);
        dev = &ctx->device();
    }

    FrameStats renderOnce(Renderer& r) {
        SceneGraph scene;
        r.render(scene);
        return r.lastFrameStats();
    }
};

// ── DeviceCapabilities ────────────────────────────────────────────────────────

TEST_F(MSAAFixture, NullBackendReportsMaxMsaaSamplesOfOne) {
    EXPECT_EQ(dev->caps().maxMsaaSamples, 1u);
}

// ── Settings ──────────────────────────────────────────────────────────────────

TEST_F(MSAAFixture, DefaultMsaaSamplesIsOne) {
    Renderer renderer(*ctx);
    EXPECT_EQ(renderer.settings().msaaSamples, 1u);
}

TEST_F(MSAAFixture, MsaaSamplesSettingRoundTrip) {
    Renderer renderer(*ctx);
    RendererSettings s = renderer.settings();
    s.msaaSamples = 4;
    renderer.setSettings(s);
    EXPECT_EQ(renderer.settings().msaaSamples, 4u);
}

// ── FrameStats clamping ───────────────────────────────────────────────────────

TEST_F(MSAAFixture, MsaaSamplesStatIsOneWhenSettingIsOne) {
    Renderer renderer(*ctx);
    FrameStats stats = renderOnce(renderer);
    EXPECT_EQ(stats.msaaSamples, 1u);
}

TEST_F(MSAAFixture, MsaaSamplesStatClampedToMaxCaps) {
    // Request 4× MSAA on Null backend (max == 1) — stat must be clamped to 1.
    Renderer renderer(*ctx);
    RendererSettings s = renderer.settings();
    s.msaaSamples = 4;
    renderer.setSettings(s);

    FrameStats stats = renderOnce(renderer);
    EXPECT_EQ(stats.msaaSamples,
              std::min(uint8_t{4}, dev->caps().maxMsaaSamples));
    EXPECT_EQ(stats.msaaSamples, 1u);  // Null clamp
}

TEST_F(MSAAFixture, MsaaSamplesStatNeverExceedsMaxCaps) {
    Renderer renderer(*ctx);
    RendererSettings s = renderer.settings();
    s.msaaSamples = 8;
    renderer.setSettings(s);

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
