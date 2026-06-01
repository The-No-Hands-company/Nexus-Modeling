// ─────────────────────────────────────────────────────────────────────────────
//  Tests: RT pipeline creation contract (Null backend)
//
//  Verifies IDevice::createRayTracingPipeline and related RT resource APIs
//  using the Null backend. All tests run in headless CI.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Renderer.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;

namespace {

struct RTPipelineFixture : public ::testing::Test {
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
};

} // namespace

TEST_F(RTPipelineFixture, DefaultPipelineHandleIsInvalid)
{
    PipelineHandle h{};
    EXPECT_FALSE(h.valid());
}

TEST_F(RTPipelineFixture, NullBackendCreateRayTracingPipelineReturnsValidHandle)
{
    RayTracingPipelineDesc desc{};
    desc.maxRecursionDepth = 1;
    desc.debugName = "test.rtp.null";

    const PipelineHandle h = dev->createRayTracingPipeline(desc);
    EXPECT_TRUE(h.valid());
    dev->destroyPipeline(h);
}

TEST_F(RTPipelineFixture, MultipleRayTracingPipelinesHaveDistinctHandles)
{
    RayTracingPipelineDesc d1{};
    d1.debugName = "rtp.a";
    RayTracingPipelineDesc d2{};
    d2.debugName = "rtp.b";

    const PipelineHandle h1 = dev->createRayTracingPipeline(d1);
    const PipelineHandle h2 = dev->createRayTracingPipeline(d2);

    EXPECT_TRUE(h1.valid());
    EXPECT_TRUE(h2.valid());
    EXPECT_NE(h1.id, h2.id);

    dev->destroyPipeline(h1);
    dev->destroyPipeline(h2);
}

TEST_F(RTPipelineFixture, RayTracingPipelineHandleRoundTripOnRenderer)
{
    RayTracingPipelineDesc desc{};
    desc.maxRecursionDepth = 2;
    desc.debugName = "rtp.roundtrip";

    const PipelineHandle h = dev->createRayTracingPipeline(desc);
    ASSERT_TRUE(h.valid());

    nexus::render::Renderer renderer(*ctx, *sc);
    EXPECT_FALSE(renderer.rayTracingPipeline().valid()); // default invalid

    renderer.setRayTracingPipeline(h);
    EXPECT_EQ(renderer.rayTracingPipeline().id, h.id);

    renderer.setRayTracingPipeline({});
    EXPECT_FALSE(renderer.rayTracingPipeline().valid());

    dev->destroyPipeline(h);
}

TEST_F(RTPipelineFixture, RayTracingPipelineDescWithMaxRecursionDepthZeroIsAccepted)
{
    // maxRecursionDepth = 0 is valid on Null backend (implementation-defined semantics).
    RayTracingPipelineDesc desc{};
    desc.maxRecursionDepth = 0;
    desc.debugName = "rtp.recurse0";

    const PipelineHandle h = dev->createRayTracingPipeline(desc);
    EXPECT_TRUE(h.valid());
    dev->destroyPipeline(h);
}

TEST_F(RTPipelineFixture, NullBackendBuildBLASReturnsValidHandle)
{
    // Null backend BLAS is a stub — just verifies it returns a non-null handle.
    BufferDesc vbDesc{};
    vbDesc.sizeBytes = 36;  // 3 vertices × 12 bytes (vec3)
    vbDesc.usage     = BufferUsage::StorageBuffer;
    vbDesc.memory    = MemoryHint::GpuOnly;
    const BufferHandle vb = dev->createBuffer(vbDesc);

    BufferDesc ibDesc{};
    ibDesc.sizeBytes = 12;  // 3 indices × 4 bytes
    ibDesc.usage     = BufferUsage::StorageBuffer;
    ibDesc.memory    = MemoryHint::GpuOnly;
    const BufferHandle ib = dev->createBuffer(ibDesc);

    ASSERT_TRUE(vb.valid());
    ASSERT_TRUE(ib.valid());

    const AccelStructHandle blas = dev->buildBLAS(vb, ib, 3, 3);
    EXPECT_TRUE(blas.valid());

    dev->destroyAccelStruct(blas);
    dev->destroyBuffer(vb);
    dev->destroyBuffer(ib);
}

TEST_F(RTPipelineFixture, NullBackendBuildTLASFromSingleBLASReturnsValidHandle)
{
    BufferDesc vbDesc{};
    vbDesc.sizeBytes = 36;
    vbDesc.usage     = BufferUsage::StorageBuffer;
    vbDesc.memory    = MemoryHint::GpuOnly;
    const BufferHandle vb = dev->createBuffer(vbDesc);

    BufferDesc ibDesc{};
    ibDesc.sizeBytes = 12;
    ibDesc.usage     = BufferUsage::StorageBuffer;
    ibDesc.memory    = MemoryHint::GpuOnly;
    const BufferHandle ib = dev->createBuffer(ibDesc);

    ASSERT_TRUE(vb.valid());
    ASSERT_TRUE(ib.valid());

    const AccelStructHandle blas = dev->buildBLAS(vb, ib, 3, 3);
    ASSERT_TRUE(blas.valid());

    const AccelStructHandle tlas = dev->buildTLAS(std::array{blas});
    EXPECT_TRUE(tlas.valid());

    dev->destroyAccelStruct(tlas);
    dev->destroyAccelStruct(blas);
    dev->destroyBuffer(vb);
    dev->destroyBuffer(ib);
}
