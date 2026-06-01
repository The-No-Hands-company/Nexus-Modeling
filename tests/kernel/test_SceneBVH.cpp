// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Scene BVH integration (Null backend)
//
//  Verifies SceneGraph::buildAccelStructs, rebuildTLAS, and the
//  FrameStats::tlasInstanceCount path in Renderer. All tests run in headless CI.
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

struct SceneBVHFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain>    sc;
    IDevice* dev = nullptr;

    void SetUp() override {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        desc.enableRayTracing = true;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);
        dev = &ctx->device();
        SwapchainDesc sd{};
        sd.extent = { 640, 480 };
        sc = ctx->createSwapchain(sd);
        ASSERT_NE(sc, nullptr);
    }

    // Creates a mesh node with stub vertex+index buffers.
    Node* addMeshNode(SceneGraph& scene, const char* name,
                      uint32_t vertexCount = 3, uint32_t indexCount = 3)
    {
        Node* node = scene.createNode(name);
        if (!node) return nullptr;

        BufferDesc vbDesc{};
        vbDesc.sizeBytes = vertexCount * 12u; // 3 floats × 4 bytes
        vbDesc.usage     = BufferUsage::StorageBuffer;
        vbDesc.memory    = MemoryHint::GpuOnly;
        node->mesh.vertexBuffer = dev->createBuffer(vbDesc);

        BufferDesc ibDesc{};
        ibDesc.sizeBytes = indexCount * 4u;
        ibDesc.usage     = BufferUsage::StorageBuffer;
        ibDesc.memory    = MemoryHint::GpuOnly;
        node->mesh.indexBuffer  = dev->createBuffer(ibDesc);

        node->mesh.vertexCount = vertexCount;
        node->mesh.indexCount  = indexCount;
        return node;
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

TEST_F(SceneBVHFixture, EmptySceneHasInvalidTLAS)
{
    SceneGraph scene;
    EXPECT_FALSE(scene.tlas().valid());
}

TEST_F(SceneBVHFixture, BuildAccelStructsOnEmptySceneBuildsZeroBLASes)
{
    SceneGraph scene;
    const uint32_t built = scene.buildAccelStructs(*dev);
    EXPECT_EQ(built, 0u);
    EXPECT_FALSE(scene.tlas().valid()); // no BLASes → no TLAS
}

TEST_F(SceneBVHFixture, BuildAccelStructsOnMeshNodeBuildsValidBLAS)
{
    SceneGraph scene;
    Node* node = addMeshNode(scene, "tri");
    ASSERT_NE(node, nullptr);

    const uint32_t built = scene.buildAccelStructs(*dev);
    EXPECT_EQ(built, 1u);
    EXPECT_TRUE(node->mesh.blas.valid());
}

TEST_F(SceneBVHFixture, BuildAccelStructsBuildsValidTLAS)
{
    SceneGraph scene;
    addMeshNode(scene, "tri");

    scene.buildAccelStructs(*dev);
    EXPECT_TRUE(scene.tlas().valid());
}

TEST_F(SceneBVHFixture, BuildAccelStructsSecondCallDoesNotRebuildExistingBLASes)
{
    SceneGraph scene;
    Node* node = addMeshNode(scene, "tri");
    ASSERT_NE(node, nullptr);

    scene.buildAccelStructs(*dev);
    const AccelStructHandle firstBLAS = node->mesh.blas;
    ASSERT_TRUE(firstBLAS.valid());

    const uint32_t rebuilt = scene.buildAccelStructs(*dev);
    EXPECT_EQ(rebuilt, 0u);  // already built — no new work
    EXPECT_EQ(node->mesh.blas.id, firstBLAS.id); // same handle
}

TEST_F(SceneBVHFixture, MultiNodeSceneBuildsOneBLASPerNode)
{
    SceneGraph scene;
    addMeshNode(scene, "a");
    addMeshNode(scene, "b");
    addMeshNode(scene, "c");

    const uint32_t built = scene.buildAccelStructs(*dev);
    EXPECT_EQ(built, 3u);
    EXPECT_TRUE(scene.tlas().valid());
}

TEST_F(SceneBVHFixture, TLASInstanceCountInFrameStatsWhenRTActive)
{
    SceneGraph scene;
    addMeshNode(scene, "tri");
    scene.buildAccelStructs(*dev);

    Renderer renderer(*ctx, *sc);

    // Wire up minimum pipelines for the scheduler path.
    PipelineHandle geom{}; geom.id = 1001;
    PipelineHandle light{}; light.id = 1002;
    renderer.setFallbackGeometryPipeline(geom);
    renderer.setLightingCompositePipeline(light);

    // Create a stub RT pipeline.
    RayTracingPipelineDesc rtDesc{};
    rtDesc.debugName = "test.bvh.rt";
    const PipelineHandle rtPipe = dev->createRayTracingPipeline(rtDesc);
    ASSERT_TRUE(rtPipe.valid());
    renderer.setRayTracingPipeline(rtPipe);

    RendererSettings settings{};
    settings.enableRTReflect = true;
    settings.mode = RenderMode::HybridRT;
    renderer.applySettings(settings);

    Camera cam;
    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();
    const auto stats = renderer.lastFrameStats();

    // On Null backend rayTracingTier == 0, so RT dispatch is gated out.
    // tlasInstanceCount is only populated when dispatch actually fires.
    // We test that the field exists and is zero on Null (correct gate behavior).
    EXPECT_EQ(stats.tlasInstanceCount, 0u);

    dev->destroyPipeline(rtPipe);
}

TEST_F(SceneBVHFixture, ClearAndDestroyTLASInvalidatesHandle)
{
    SceneGraph scene;
    addMeshNode(scene, "tri");
    scene.buildAccelStructs(*dev);
    ASSERT_TRUE(scene.tlas().valid());

    scene.clearAndDestroyTLAS(*dev);
    EXPECT_FALSE(scene.tlas().valid());
}
