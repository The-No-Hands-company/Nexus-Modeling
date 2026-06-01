// ─────────────────────────────────────────────────────────────────────────────
//  Tests: RT Shader Binding Tables (v0.6 Track 3)
//
//  Verifies ShaderBindingTableDesc, IDevice::createShaderBindingTable,
//  Renderer::setShaderBindingTable, and traceRaysWithSBT dispatch.
//  All tests run on Null backend in headless CI.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct SBTFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    IDevice* dev = nullptr;

    void SetUp() override {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        desc.enableRayTracing = true;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);
        dev = &ctx->device();
    }
};

// ── createShaderBindingTable ──────────────────────────────────────────────────

TEST_F(SBTFixture, CreateSBTReturnsValidHandleOnNullBackend) {
    ShaderBindingTableDesc desc{};
    SBTHandle sbt = dev->createShaderBindingTable(desc);
    EXPECT_TRUE(sbt.valid());
    dev->freeSBT(sbt);
}

TEST_F(SBTFixture, CreateSBTWithPipelineHandleDoesNotCrash) {
    RayTracingPipelineDesc rtDesc{};
    PipelineHandle pipeline = dev->createRayTracingPipeline(rtDesc);
    EXPECT_TRUE(pipeline.valid());

    ShaderBindingTableDesc sbtDesc{};
    sbtDesc.pipeline  = pipeline;
    sbtDesc.debugName = "test.sbt";

    SBTHandle sbt = dev->createShaderBindingTable(sbtDesc);
    EXPECT_TRUE(sbt.valid());

    dev->freeSBT(sbt);
    dev->destroyPipeline(pipeline);
}

TEST_F(SBTFixture, TwoSBTHandlesAreDistinct) {
    ShaderBindingTableDesc desc{};
    SBTHandle a = dev->createShaderBindingTable(desc);
    SBTHandle b = dev->createShaderBindingTable(desc);
    EXPECT_TRUE(a.valid());
    EXPECT_TRUE(b.valid());
    EXPECT_NE(a.id, b.id);
    dev->freeSBT(a);
    dev->freeSBT(b);
}

// ── Renderer SBT slot ─────────────────────────────────────────────────────────

TEST_F(SBTFixture, RendererDefaultSBTIsInvalid) {
    Renderer renderer(*ctx);
    EXPECT_FALSE(renderer.shaderBindingTable().valid());
}

TEST_F(SBTFixture, SetShaderBindingTableRoundTrip) {
    ShaderBindingTableDesc desc{};
    SBTHandle sbt = dev->createShaderBindingTable(desc);
    ASSERT_TRUE(sbt.valid());

    Renderer renderer(*ctx);
    renderer.setShaderBindingTable(sbt);
    EXPECT_EQ(renderer.shaderBindingTable().id, sbt.id);

    dev->freeSBT(sbt);
}

TEST_F(SBTFixture, ClearSBTSlotWithInvalidHandle) {
    ShaderBindingTableDesc desc{};
    SBTHandle sbt = dev->createShaderBindingTable(desc);
    Renderer renderer(*ctx);
    renderer.setShaderBindingTable(sbt);
    EXPECT_TRUE(renderer.shaderBindingTable().valid());

    renderer.setShaderBindingTable({});  // clear
    EXPECT_FALSE(renderer.shaderBindingTable().valid());
    dev->freeSBT(sbt);
}

// ── traceRaysWithSBT dispatch ─────────────────────────────────────────────────

TEST_F(SBTFixture, RendererWithSBTRenderDoesNotCrash) {
    // Set up RT conditions on Null backend — RT caps are off so rtReflectionsActive
    // will be false, but the code path exercising SBT selection must not crash.
    ShaderBindingTableDesc sbtDesc{};
    SBTHandle sbt = dev->createShaderBindingTable(sbtDesc);

    Renderer renderer(*ctx);
    renderer.setShaderBindingTable(sbt);

    RendererSettings settings{};
    settings.enableRTReflect = true;
    settings.mode            = RenderMode::HybridRT;
    renderer.applySettings(settings);

    SceneGraph scene;
    EXPECT_NO_THROW(renderer.render(scene));

    dev->freeSBT(sbt);
}

TEST_F(SBTFixture, SBTSlotDoesNotAffectRasterFrameStats) {
    // SBT should have no effect on raster draw calls — it is only consulted in
    // the RT dispatch block (which is gated off on Null backend).
    ShaderBindingTableDesc sbtDesc{};
    SBTHandle sbt = dev->createShaderBindingTable(sbtDesc);

    Renderer renderer(*ctx);
    renderer.setShaderBindingTable(sbt);

    SceneGraph scene;
    renderer.render(scene);
    const auto stats = renderer.lastFrameStats();

    EXPECT_EQ(stats.rtReflectionsActive, false);

    dev->freeSBT(sbt);
}

}  // namespace
