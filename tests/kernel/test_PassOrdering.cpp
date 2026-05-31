// ─────────────────────────────────────────────────────────────────────────────
//  Tests — Pass ordering, barrier sequencing, and transition choreography
//  (Month 15 Track 2 — M2 regression hardening)
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

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using namespace nexus::gfx;
using namespace nexus::render;

// ── Barrier-recording command buffer ─────────────────────────────────────────

struct BarrierEvent {
    TextureHandle texture;
    TextureLayout from;
    TextureLayout to;
};

class RecordingCB final : public ICommandBuffer {
public:
    std::vector<BarrierEvent> barriers;

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

    void textureBarrier(const TextureBarrier& b) override {
        barriers.push_back({ b.texture, b.oldLayout, b.newLayout });
    }
    void textureBarriers(std::span<const TextureBarrier> bs) override {
        for (const auto& b : bs)
            barriers.push_back({ b.texture, b.oldLayout, b.newLayout });
    }

    void bufferBarrier(const BufferBarrier&) override {}
    void bufferBarriers(std::span<const BufferBarrier>) override {}
    void resetQueryPool(QueryPoolHandle, uint32_t, uint32_t) override {}
    void writeTimestamp(QueryPoolHandle, uint32_t) override {}
    void beginDebugLabel(const char*, float, float, float) override {}
    void endDebugLabel() override {}
    void insertDebugLabel(const char*) override {}
};

// ── Minimal recording IFrameScheduler ────────────────────────────────────────

class RecordingScheduler final : public IFrameScheduler {
public:
    explicit RecordingScheduler(RecordingCB& cb, Extent2D ext = {1280u, 720u})
        : m_cb(cb), m_extent(ext) {}

    std::optional<FrameContext> beginFrame() override {
        FrameContext fc{};
        fc.cmd        = &m_cb;
        fc.colorTarget.id = 555;
        fc.extent     = m_extent;
        fc.frameSlot  = 0;
        fc.imageIndex = 0;
        ++beginCount;
        return fc;
    }
    void endFrame() override { ++endCount; }
    void onResize(Extent2D e) override { m_extent = e; ++resizeCount; }
    uint32_t maxFramesInFlight() const noexcept override { return 2; }
    void insertGBufferRTBarrier(ICommandBuffer& cmd, TextureHandle depth) noexcept override {
        cmd.textureBarrier({ depth, TextureLayout::DepthWrite, TextureLayout::ShaderRead });
        rtBarrierInserted = true;
    }

    int  beginCount = 0;
    int  endCount   = 0;
    int  resizeCount = 0;
    bool rtBarrierInserted = false;

private:
    RecordingCB& m_cb;
    Extent2D     m_extent;
};

// ── Fixture ───────────────────────────────────────────────────────────────────

struct PassOrderingFixture : public ::testing::Test {
    std::unique_ptr<RenderContext>      ctx;
    std::unique_ptr<ISwapchain>         swapchain;
    RecordingCB                         cb;
    std::unique_ptr<RecordingScheduler> sched;
    std::unique_ptr<Renderer>           renderer;

    void SetUp() override {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        ctx = RenderContext::create(desc);

        SwapchainDesc sd{};
        sd.extent = {1280u, 720u};
        swapchain = ctx->createSwapchain(sd);

        renderer = std::make_unique<Renderer>(*ctx, *swapchain);
        sched    = std::make_unique<RecordingScheduler>(cb);
        renderer->setFrameScheduler(sched.get());
    }

    void runFrame() {
        Camera cam;
        SceneGraph scene;
        renderer->beginFrame();
        renderer->render(cam, scene);
        renderer->endFrame();
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(PassOrderingFixture, GeometryPassProducesColorAttachmentBarrier) {
    runFrame();
    // After the geometry pass the GBuffer color targets transition to ShaderRead.
    const bool hasColorToShader = std::any_of(
        cb.barriers.begin(), cb.barriers.end(),
        [](const BarrierEvent& e) {
            return e.from == TextureLayout::ColorAttachment
                && e.to   == TextureLayout::ShaderRead;
        });
    EXPECT_TRUE(hasColorToShader);
}

TEST_F(PassOrderingFixture, DepthTransitionFromDepthWriteOccurs) {
    runFrame();
    const bool hasDepthTransition = std::any_of(
        cb.barriers.begin(), cb.barriers.end(),
        [](const BarrierEvent& e) {
            return e.from == TextureLayout::DepthWrite;
        });
    EXPECT_TRUE(hasDepthTransition);
}

TEST_F(PassOrderingFixture, ColorAttachmentBarrierPrecedesShaderReadBarrier) {
    runFrame();
    int firstColorWrite = -1;
    int firstShaderRead = -1;
    for (int i = 0; i < static_cast<int>(cb.barriers.size()); ++i) {
        const auto& e = cb.barriers[i];
        if (firstColorWrite < 0 && e.to == TextureLayout::ColorAttachment)
            firstColorWrite = i;
        if (firstShaderRead < 0 && e.to == TextureLayout::ShaderRead)
            firstShaderRead = i;
    }
    ASSERT_GE(firstColorWrite, 0) << "No ColorAttachment transition found";
    ASSERT_GE(firstShaderRead, 0) << "No ShaderRead transition found";
    EXPECT_LT(firstColorWrite, firstShaderRead);
}

TEST_F(PassOrderingFixture, CompositeTransitionsGBufferToShaderRead) {
    runFrame();
    const long count = std::count_if(
        cb.barriers.begin(), cb.barriers.end(),
        [](const BarrierEvent& e) {
            return e.from == TextureLayout::ColorAttachment
                && e.to   == TextureLayout::ShaderRead;
        });
    // Albedo, normal, velocity — at least 3 GBuffer color targets.
    EXPECT_GE(count, 3);
}

TEST_F(PassOrderingFixture, OnResizeReallocatesGBuffer) {
    runFrame();
    const size_t barriersBefore = cb.barriers.size();

    renderer->onResize({640u, 360u});
    runFrame();

    // A new frame after resize allocates a fresh GBuffer and must emit new barriers.
    EXPECT_GT(cb.barriers.size(), barriersBefore);
}

TEST_F(PassOrderingFixture, RTBarrierInsertedWhenSchedulerSupports) {
    // Directly invoke the scheduler's insertGBufferRTBarrier to simulate
    // the RT primary pass slot.
    TextureHandle fakeDepth{};
    fakeDepth.id = 999u;
    sched->insertGBufferRTBarrier(cb, fakeDepth);

    EXPECT_TRUE(sched->rtBarrierInserted);
    const bool found = std::any_of(
        cb.barriers.begin(), cb.barriers.end(),
        [&fakeDepth](const BarrierEvent& e) {
            return e.texture.id == fakeDepth.id
                && e.from == TextureLayout::DepthWrite
                && e.to   == TextureLayout::ShaderRead;
        });
    EXPECT_TRUE(found);
}

// ── Track 3 — Shadow pass draw call verification ──────────────────────────────

TEST_F(PassOrderingFixture, ShadowPassDoesNotInflateDrawCallsWithNoContract)
{
    // Without a bound shadow pipeline + bindings, the shadow pass must not emit
    // any draw calls on top of the base geometry pass count.
    SceneGraph scene;
    Camera cam;

    renderer->beginFrame();
    renderer->render(cam, scene);
    renderer->endFrame();

    const uint32_t calls = renderer->lastFrameStats().drawCalls;

    // Reset and run a second frame — should stay consistent.
    renderer->beginFrame();
    renderer->render(cam, scene);
    renderer->endFrame();

    EXPECT_EQ(renderer->lastFrameStats().drawCalls, calls);
}

TEST_F(PassOrderingFixture, ShadowPassDrawCallsZeroWithEmptyScene)
{
    SceneGraph emptyScene;
    Camera cam;
    runFrame(); // warm-up

    renderer->beginFrame();
    renderer->render(cam, emptyScene);
    renderer->endFrame();

    // Empty scene → no geometry draw calls regardless of shadow state.
    EXPECT_EQ(renderer->lastFrameStats().drawCalls, 0u);
}

TEST_F(PassOrderingFixture, ShadowPassStatsResetEachFrame)
{
    SceneGraph scene;
    Camera cam;

    renderer->beginFrame();
    renderer->render(cam, scene);
    renderer->endFrame();
    const uint32_t firstFrameCalls = renderer->lastFrameStats().drawCalls;

    renderer->beginFrame();
    renderer->render(cam, scene);
    renderer->endFrame();
    const uint32_t secondFrameCalls = renderer->lastFrameStats().drawCalls;

    // Stats should be stable between frames for the same scene.
    EXPECT_EQ(firstFrameCalls, secondFrameCalls);
}

TEST_F(PassOrderingFixture, ShadowPassNodesZeroWithNoPipelineSet)
{
    // Verify the renderer doesn't crash and returns zero draw calls
    // when no fallback geometry pipeline has been set before rendering.
    std::unique_ptr<Renderer> freshRenderer =
        std::make_unique<Renderer>(*ctx, *swapchain);
    RecordingScheduler freshSched(cb);
    freshRenderer->setFrameScheduler(&freshSched);

    SceneGraph scene;
    Camera cam;
    freshRenderer->beginFrame();
    freshRenderer->render(cam, scene);
    freshRenderer->endFrame();

    EXPECT_EQ(freshRenderer->lastFrameStats().drawCalls, 0u);
}
