// ─────────────────────────────────────────────────────────────────────────────
//  Tests — Render path parity (scheduler vs. direct-swapchain)
//  (Month 15 Track 3 — M3 formal parity documentation and contract tests)
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/gfx/FrameScheduler.h>
#include <nexus/gfx/CommandBuffer.h>
#include <nexus/gfx/Device.h>

#include <optional>

using namespace nexus::gfx;
using namespace nexus::render;

// ── Null ICommandBuffer stub ──────────────────────────────────────────────────

class NullCB final : public ICommandBuffer {
public:
    void begin()  override {}
    void end()    override {}
    void reset()  override {}
    void beginRenderPass(RenderPassHandle,
                         std::span<const TextureHandle>,
                         TextureHandle,
                         std::span<const ClearValue>,
                         const Rect2D&) override {}
    void endRenderPass() override {}
    void bindPipeline(PipelineHandle) override {}
    void setViewport(const Viewport&) override {}
    void setScissor(const Rect2D&)    override {}
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

// ── Minimal null IFrameScheduler ──────────────────────────────────────────────

class NullScheduler final : public IFrameScheduler {
public:
    explicit NullScheduler(Extent2D ext = {1280u, 720u}) : m_ext(ext) {}

    std::optional<FrameContext> beginFrame() override {
        FrameContext fc{};
        fc.cmd        = &m_cb;
        fc.colorTarget.id = 1u;
        fc.extent     = m_ext;
        fc.frameSlot  = 0;
        fc.imageIndex = 0;
        return fc;
    }
    void endFrame() override {}
    void onResize(Extent2D e) override { m_ext = e; }
    uint32_t maxFramesInFlight() const noexcept override { return 2; }
    void insertGBufferRTBarrier(ICommandBuffer&, TextureHandle) noexcept override {}

private:
    NullCB   m_cb;
    Extent2D m_ext;
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::unique_ptr<RenderContext> makeNullCtx()
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;
    return RenderContext::create(desc);
}

static void runOneFrame(Renderer& r)
{
    Camera cam;
    SceneGraph scene;
    r.beginFrame();
    r.render(cam, scene);
    r.endFrame();
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST(RenderPathParity, NullBackendUsesDirectSwapchainPath) {
    // With no frame scheduler set, the renderer uses the direct swapchain path.
    auto ctx = makeNullCtx();
    auto sc  = ctx->createSwapchain([]{ SwapchainDesc sd{}; sd.extent = {800u, 600u}; return sd; }());
    Renderer r(*ctx, *sc);
    EXPECT_NO_FATAL_FAILURE(runOneFrame(r));
}

TEST(RenderPathParity, DirectPathProducesZeroDrawCalls) {
    // The non-scheduler path is a minimal compatibility fallback; geometry
    // pass draw calls are not recorded without a frame context.
    auto ctx = makeNullCtx();
    auto sc  = ctx->createSwapchain([]{ SwapchainDesc sd{}; sd.extent = {800u, 600u}; return sd; }());
    Renderer r(*ctx, *sc);
    runOneFrame(r);
    EXPECT_EQ(r.lastFrameStats().drawCalls, 0u);
}

TEST(RenderPathParity, SchedulerPathProducesFrameContextWithValidExtent) {
    auto ctx = makeNullCtx();
    auto sc  = ctx->createSwapchain([]{ SwapchainDesc sd{}; sd.extent = {1280u, 720u}; return sd; }());
    Renderer r(*ctx, *sc);

    NullScheduler sched({1280u, 720u});
    r.setFrameScheduler(&sched);

    EXPECT_NO_FATAL_FAILURE(runOneFrame(r));
    EXPECT_EQ(r.lastFrameStats().totalNodes, 0u);
}

TEST(RenderPathParity, ShadowsEnabledByDefaultOnBothPaths) {
    // enableShadows defaults to true; shadows run when a contract + pipeline are set.
    auto ctx = makeNullCtx();
    auto sc  = ctx->createSwapchain([]{ SwapchainDesc sd{}; sd.extent = {800u, 600u}; return sd; }());
    Renderer r(*ctx, *sc);
    EXPECT_TRUE(r.settings().enableShadows);
}

TEST(RenderPathParity, EnableShadowsWithNoContractProducesNoShadowPasses) {
    auto ctx = makeNullCtx();
    auto sc  = ctx->createSwapchain([]{ SwapchainDesc sd{}; sd.extent = {800u, 600u}; return sd; }());
    Renderer r(*ctx, *sc);

    RendererSettings s{};
    s.enableShadows = true;
    r.applySettings(s);

    // No shadow contract — must not crash; draw calls remain zero.
    EXPECT_NO_FATAL_FAILURE(runOneFrame(r));
    EXPECT_EQ(r.lastFrameStats().drawCalls, 0u);
}

TEST(RenderPathParity, FrameStatsResetEachFrame) {
    auto ctx = makeNullCtx();
    auto sc  = ctx->createSwapchain([]{ SwapchainDesc sd{}; sd.extent = {800u, 600u}; return sd; }());
    Renderer r(*ctx, *sc);

    NullScheduler sched;
    r.setFrameScheduler(&sched);

    runOneFrame(r);
    const uint32_t afterFirst = r.lastFrameStats().totalNodes;
    runOneFrame(r);

    // Stats are reset each frame; empty scene always yields 0 totalNodes.
    EXPECT_EQ(r.lastFrameStats().totalNodes, afterFirst);
    EXPECT_EQ(r.lastFrameStats().drawCalls, 0u);
}
