// ─────────────────────────────────────────────────────────────────────────────
//  Tests for RT/mesh-shader production path — M4 completion (Month 22)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Renderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/RenderContext.h>

#include <gtest/gtest.h>

using namespace nexus::render;
using namespace nexus::gfx;

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace {

RenderContextDesc nullCtxDesc()
{
    RenderContextDesc d{};
    d.preferredBackend = Backend::Null;
    d.validation       = ValidationLevel::Off;
    return d;
}

struct RTPathFixture : ::testing::Test {
    void SetUp() override
    {
        ctx = RenderContext::create(nullCtxDesc());
        ASSERT_TRUE(ctx);
        SwapchainDesc sd{};
        sd.extent = {800u, 600u};
        sc = ctx->createSwapchain(sd);
        ASSERT_TRUE(sc);
        renderer = std::make_unique<Renderer>(*ctx, *sc);
    }

    void renderOneFrame()
    {
        renderer->beginFrame();
        renderer->render(cam, scene);
        renderer->endFrame();
    }

    std::shared_ptr<RenderContext> ctx;
    std::shared_ptr<ISwapchain>    sc;
    std::unique_ptr<Renderer>      renderer;
    Camera                         cam;
    SceneGraph                     scene;
};

} // namespace

// ── Active render mode tracking ───────────────────────────────────────────────

TEST_F(RTPathFixture, DefaultActiveRenderModeIsRasterize)
{
    renderOneFrame();
    EXPECT_EQ(renderer->lastFrameStats().activeRenderMode, RenderMode::Rasterize);
}

TEST_F(RTPathFixture, PathTraceModeDowngradesToRasterizeOnNullBackend)
{
    // Null backend has rayTracingTier=0; PathTrace must downgrade.
    RendererSettings s = renderer->settings();
    s.mode = RenderMode::PathTrace;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_EQ(renderer->lastFrameStats().activeRenderMode, RenderMode::Rasterize);
}

TEST_F(RTPathFixture, HybridRTModeDowngradesToRasterizeOnNullBackend)
{
    RendererSettings s = renderer->settings();
    s.mode = RenderMode::HybridRT;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_EQ(renderer->lastFrameStats().activeRenderMode, RenderMode::Rasterize);
}

TEST_F(RTPathFixture, RasterizeModeRemainsRasterize)
{
    RendererSettings s = renderer->settings();
    s.mode = RenderMode::Rasterize;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_EQ(renderer->lastFrameStats().activeRenderMode, RenderMode::Rasterize);
}

TEST_F(RTPathFixture, ActiveRenderModeIsStableAcrossMultipleFrames)
{
    // Mode downgrade must not oscillate across frames.
    RendererSettings s = renderer->settings();
    s.mode = RenderMode::PathTrace;
    renderer->applySettings(s);

    for (int i = 0; i < 4; ++i) {
        renderOneFrame();
        EXPECT_EQ(renderer->lastFrameStats().activeRenderMode, RenderMode::Rasterize)
            << "frame " << i;
    }
}

// ── RT pipeline API ───────────────────────────────────────────────────────────

TEST_F(RTPathFixture, RTPipelineDefaultIsInvalid)
{
    EXPECT_FALSE(renderer->rayTracingPipeline().valid());
}

TEST_F(RTPathFixture, RTPipelineRoundTrip)
{
    PipelineHandle rtPipe{};
    rtPipe.id = 999;
    renderer->setRayTracingPipeline(rtPipe);
    EXPECT_EQ(renderer->rayTracingPipeline().id, 999u);
}

TEST_F(RTPathFixture, ClearRTPipelineStoresInvalidHandle)
{
    PipelineHandle rtPipe{};
    rtPipe.id = 999;
    renderer->setRayTracingPipeline(rtPipe);
    renderer->setRayTracingPipeline(PipelineHandle{});
    EXPECT_FALSE(renderer->rayTracingPipeline().valid());
}

// ── RT reflections — inactive on Null backend ─────────────────────────────────

TEST_F(RTPathFixture, RTReflectionsInactiveByDefault)
{
    renderOneFrame();
    EXPECT_FALSE(renderer->lastFrameStats().rtReflectionsActive);
}

TEST_F(RTPathFixture, RTReflectionsInactiveOnNullBackendEvenWhenEnabled)
{
    // enableRTReflect=true but Null backend has no RT caps → must not activate.
    PipelineHandle rtPipe{};
    rtPipe.id = 888;
    renderer->setRayTracingPipeline(rtPipe);

    RendererSettings s = renderer->settings();
    s.enableRTReflect = true;
    s.mode = RenderMode::HybridRT;  // will downgrade to Rasterize on Null
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_FALSE(renderer->lastFrameStats().rtReflectionsActive);
}

TEST_F(RTPathFixture, RTReflectionsRequireValidRTPipeline)
{
    // Even if RT caps were present (they aren't on Null), no pipeline → inactive.
    RendererSettings s = renderer->settings();
    s.enableRTReflect = true;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_FALSE(renderer->lastFrameStats().rtReflectionsActive);
}

TEST_F(RTPathFixture, RTReflectionsInactiveDoesNotAffectDrawCallCount)
{
    RendererSettings s = renderer->settings();
    s.enableRTReflect = true;
    renderer->applySettings(s);

    renderOneFrame();
    // RT reflections gate must not spuriously add draw calls.
    EXPECT_EQ(renderer->lastFrameStats().drawCalls, 0u);
}

// ── Mode downgrade preserves correctness of other stats ─────────────────────

TEST_F(RTPathFixture, PathTraceDowngradePreservesDenoisingStats)
{
    // Denoiser should still work even when RT mode was requested and downgraded.
    // (Tests interaction between mode-gate and denoiser gate.)
    RendererSettings s = renderer->settings();
    s.mode = RenderMode::PathTrace;
    renderer->applySettings(s);

    renderOneFrame();
    // No neural renderer attached → denoisingActive must be false regardless of mode.
    EXPECT_FALSE(renderer->lastFrameStats().denoisingActive);
    // But activeRenderMode should reflect the downgraded value.
    EXPECT_EQ(renderer->lastFrameStats().activeRenderMode, RenderMode::Rasterize);
}
