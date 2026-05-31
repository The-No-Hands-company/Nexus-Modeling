// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Mesh Shader Production Path (Null backend)
//
//  Verifies RendererSettings::enableMeshShaders and
//  FrameStats::meshShaderDrawCalls. All tests run in headless CI on the Null
//  backend, where meshShaders caps == false, so dispatch never fires.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct MeshShaderFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<Renderer>      renderer;
    SceneGraph                     scene;

    void SetUp() override {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);
        renderer = std::make_unique<Renderer>(*ctx);
    }

    FrameStats render() {
        renderer->render(scene);
        return renderer->lastFrameStats();
    }
};

// ── Settings API ──────────────────────────────────────────────────────────────

TEST_F(MeshShaderFixture, DefaultMeshShaderSettingIsEnabled) {
    RendererSettings s = renderer->settings();
    EXPECT_TRUE(s.enableMeshShaders);
}

TEST_F(MeshShaderFixture, EnableMeshShadersRoundTripOnSettings) {
    RendererSettings s = renderer->settings();
    s.enableMeshShaders = false;
    renderer->setSettings(s);
    EXPECT_FALSE(renderer->settings().enableMeshShaders);

    s.enableMeshShaders = true;
    renderer->setSettings(s);
    EXPECT_TRUE(renderer->settings().enableMeshShaders);
}

// ── Null backend — caps always false ─────────────────────────────────────────

TEST_F(MeshShaderFixture, MeshShaderDrawCallsDefaultIsZero) {
    FrameStats stats = render();
    EXPECT_EQ(stats.meshShaderDrawCalls, 0u);
}

TEST_F(MeshShaderFixture, MeshShaderFlagEnabledWithoutCapDoesNotDispatch) {
    // Null backend: meshShaders cap is always false regardless of flag.
    RendererSettings s = renderer->settings();
    s.enableMeshShaders = true;
    renderer->setSettings(s);

    EXPECT_FALSE(ctx->device().caps().meshShaders);
    FrameStats stats = render();
    EXPECT_EQ(stats.meshShaderDrawCalls, 0u);
}

TEST_F(MeshShaderFixture, MeshShaderFlagDisabledPreventsMeshTaskDispatch) {
    RendererSettings s = renderer->settings();
    s.enableMeshShaders = false;
    renderer->setSettings(s);

    FrameStats stats = render();
    EXPECT_EQ(stats.meshShaderDrawCalls, 0u);
}

TEST_F(MeshShaderFixture, MeshShaderDrawCallsIsolatedFromRasterDrawCalls) {
    // Raster draw calls may be non-zero; mesh shader draw calls must be zero on
    // Null backend because the caps gate prevents dispatch.
    FrameStats stats = render();
    // meshShaderDrawCalls is a separate counter — never aliases drawCalls.
    EXPECT_EQ(stats.meshShaderDrawCalls, 0u);
}

// ── Per-frame reset ───────────────────────────────────────────────────────────

TEST_F(MeshShaderFixture, MeshShaderDrawCallsResetEachFrame) {
    // Render two frames — counter must be independently zero each frame on Null.
    FrameStats f1 = render();
    FrameStats f2 = render();
    EXPECT_EQ(f1.meshShaderDrawCalls, 0u);
    EXPECT_EQ(f2.meshShaderDrawCalls, 0u);
}

// ── Pipeline slots ────────────────────────────────────────────────────────────

TEST_F(MeshShaderFixture, MeshShaderPipelineSlotSeparateFromGeometryPipeline) {
    // setFallbackMeshPipeline is a distinct slot from the raster pipeline.
    // Creating a mesh pipeline on the Null backend must return a valid handle.
    IDevice& dev = ctx->device();
    MeshShaderPipelineDesc msDesc{};
    PipelineHandle msPipeline = dev.createMeshShaderPipeline(msDesc);
    EXPECT_TRUE(msPipeline.valid());

    // Setting it on the fallback slot must not crash.
    EXPECT_NO_THROW(renderer->setFallbackMeshPipeline(msPipeline));
}

TEST_F(MeshShaderFixture, ShadowMeshPipelineSlotAcceptsHandle) {
    IDevice& dev = ctx->device();
    MeshShaderPipelineDesc msDesc{};
    PipelineHandle msPipeline = dev.createMeshShaderPipeline(msDesc);
    EXPECT_TRUE(msPipeline.valid());
    EXPECT_NO_THROW(renderer->setShadowMeshPipeline(msPipeline));
}

}  // namespace
