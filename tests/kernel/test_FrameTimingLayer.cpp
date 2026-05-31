// ─────────────────────────────────────────────────────────────────────────────
//  Unit tests for FrameTimingLayer
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/FrameTimingLayer.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/CommandBuffer.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

// Minimal no-op command buffer for directly exercising FrameTimingLayer.
class NullCB final : public ICommandBuffer {
public:
    void begin() override {}
    void end()   override {}
    void reset() override {}
    void beginRenderPass(RenderPassHandle, std::span<const TextureHandle>,
                         TextureHandle, std::span<const ClearValue>, const Rect2D&) override {}
    void endRenderPass() override {}
    void bindPipeline(PipelineHandle) override {}
    void setViewport(const Viewport&)  override {}
    void setScissor(const Rect2D&)     override {}
    void bindVertexBuffer(BufferHandle, uint64_t, uint32_t) override {}
    void bindIndexBuffer(BufferHandle, uint64_t, bool)      override {}
    void pushConstants(ShaderStage, const void*, uint32_t, uint32_t) override {}
    void bindDescriptorSet(DescriptorSetHandle, uint32_t) override {}
    void draw(uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void drawIndexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {}
    void drawIndirect(BufferHandle, uint64_t, uint32_t, uint32_t) override {}
    void drawIndexedIndirect(BufferHandle, uint64_t, uint32_t, uint32_t) override {}
    void drawMeshTasks(uint32_t, uint32_t, uint32_t) override {}
    void drawMeshTasksIndirect(BufferHandle, uint64_t, uint32_t) override {}
    void dispatch(uint32_t, uint32_t, uint32_t) override {}
    void dispatchIndirect(BufferHandle, uint64_t) override {}
    void traceRays(uint32_t, uint32_t, uint32_t) override {}
    void copyBuffer(BufferHandle, BufferHandle, uint64_t, uint64_t, uint64_t) override {}
    void copyTexture(TextureHandle, TextureHandle) override {}
    void blitTexture(TextureHandle, TextureHandle, const Rect2D&, const Rect2D&) override {}
    void globalBarrier(const GlobalMemoryBarrier&) override {}
    void textureBarrier(const TextureBarrier&) override {}
    void textureBarriers(std::span<const TextureBarrier>) override {}
    void bufferBarrier(const BufferBarrier&) override {}
    void bufferBarriers(std::span<const BufferBarrier>) override {}
    void resetQueryPool(QueryPoolHandle, uint32_t, uint32_t) override {}
    void writeTimestamp(QueryPoolHandle, uint32_t) override {}
    void beginDebugLabel(const char*, float, float, float) override {}
    void endDebugLabel() override {}
    void insertDebugLabel(const char*) override {}
};

RenderContextDesc nullDesc()
{
    RenderContextDesc d{};
    d.preferredBackend = Backend::Null;
    d.validation       = ValidationLevel::Off;
    return d;
}

} // namespace

// ── Lifecycle ─────────────────────────────────────────────────────────────────

TEST(FrameTimingLayer, NotReadyBeforeCreate)
{
    FrameTimingLayer ftl;
    EXPECT_FALSE(ftl.isReady());
}

TEST(FrameTimingLayer, ReadyAfterCreate)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);
    FrameTimingLayer ftl;
    EXPECT_TRUE(ftl.create(ctx->device(), 2));
    EXPECT_TRUE(ftl.isReady());
    ftl.destroy(ctx->device());
}

TEST(FrameTimingLayer, NotReadyAfterDestroy)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);
    FrameTimingLayer ftl;
    ASSERT_TRUE(ftl.create(ctx->device(), 2));
    ftl.destroy(ctx->device());
    EXPECT_FALSE(ftl.isReady());
}

TEST(FrameTimingLayer, DoubleDestroyIsSafe)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);
    FrameTimingLayer ftl;
    ASSERT_TRUE(ftl.create(ctx->device(), 2));
    ftl.destroy(ctx->device());
    ftl.destroy(ctx->device()); // must not crash
}

TEST(FrameTimingLayer, CreateWithZeroFramesReturnsFalse)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);
    FrameTimingLayer ftl;
    EXPECT_FALSE(ftl.create(ctx->device(), 0));
    EXPECT_FALSE(ftl.isReady());
}

TEST(FrameTimingLayer, DestroyBeforeCreateIsSafe)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);
    FrameTimingLayer ftl;
    ftl.destroy(ctx->device()); // must not crash — nothing was created
}

// ── Null backend per-frame operations ────────────────────────────────────────

TEST(FrameTimingLayer, BeginEndFrameNoCrashOnNull)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);
    FrameTimingLayer ftl;
    ASSERT_TRUE(ftl.create(ctx->device(), 2));

    NullCB cmd;
    ftl.beginFrame(cmd, 0);
    ftl.markGeometryEnd(cmd, 0);
    ftl.markShadowEnd(cmd, 0);
    ftl.endFrame(cmd, 0);

    ftl.destroy(ctx->device());
}

TEST(FrameTimingLayer, ReadbackReturnsZeroOnNull)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);
    FrameTimingLayer ftl;
    ASSERT_TRUE(ftl.create(ctx->device(), 2));

    NullCB cmd;
    ftl.beginFrame(cmd, 0);
    ftl.markGeometryEnd(cmd, 0);
    ftl.markShadowEnd(cmd, 0);
    ftl.endFrame(cmd, 0);

    const FrameTimingResult r = ftl.readback(ctx->device(), 0);
    EXPECT_DOUBLE_EQ(r.totalGpuMs,      0.0);
    EXPECT_DOUBLE_EQ(r.geometryPassMs,  0.0);
    EXPECT_DOUBLE_EQ(r.shadowPassMs,    0.0);
    EXPECT_DOUBLE_EQ(r.compositePassMs, 0.0);

    ftl.destroy(ctx->device());
}

TEST(FrameTimingLayer, ReadbackBeforeEndFrameReturnsZero)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);
    FrameTimingLayer ftl;
    ASSERT_TRUE(ftl.create(ctx->device(), 2));

    NullCB cmd;
    ftl.beginFrame(cmd, 0);
    // endFrame not called — slot not marked ready
    const FrameTimingResult r = ftl.readback(ctx->device(), 0);
    EXPECT_DOUBLE_EQ(r.totalGpuMs, 0.0);

    ftl.destroy(ctx->device());
}

TEST(FrameTimingLayer, ReadbackOutOfRangeSlotIsSafe)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);
    FrameTimingLayer ftl;
    ASSERT_TRUE(ftl.create(ctx->device(), 2));

    // slot 99 is way out of range
    const FrameTimingResult r = ftl.readback(ctx->device(), 99);
    EXPECT_DOUBLE_EQ(r.totalGpuMs, 0.0);

    ftl.destroy(ctx->device());
}

// ── Renderer integration ──────────────────────────────────────────────────────

TEST(FrameTimingLayer, RendererGpuTimeMsIsNonNegativeOnNull)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);

    SwapchainDesc sd{};
    sd.extent = {800u, 600u};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_TRUE(sc);

    Renderer renderer(*ctx, *sc);

    PipelineHandle gp{};
    gp.id = 1;
    PipelineHandle lp{};
    lp.id = 2;
    renderer.setFallbackGeometryPipeline(gp);
    renderer.setLightingCompositePipeline(lp);

    (void)renderer.enableFrameTiming(2);
    EXPECT_TRUE(renderer.isFrameTimingEnabled());

    SceneGraph scene;
    Camera cam;
    cam.setPerspective(60.f, 800.f / 600.f, 0.1f, 100.f);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const double gpuMs = renderer.lastFrameTimingResult().totalGpuMs;
    EXPECT_GE(gpuMs, 0.0); // zero on Null — never negative
}

TEST(FrameTimingLayer, DisableTimingClearsReadyState)
{
    auto ctx = RenderContext::create(nullDesc());
    ASSERT_TRUE(ctx);

    SwapchainDesc sd{};
    sd.extent = {800u, 600u};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_TRUE(sc);

    Renderer renderer(*ctx, *sc);

    (void)renderer.enableFrameTiming(2);
    EXPECT_TRUE(renderer.isFrameTimingEnabled());

    renderer.disableFrameTiming();
    EXPECT_FALSE(renderer.isFrameTimingEnabled());
}
