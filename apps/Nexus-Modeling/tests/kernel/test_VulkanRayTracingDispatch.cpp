// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Vulkan ray-tracing bring-up.
//
//  - ShadersCompileToModules runs anywhere a Vulkan device exists (GLSL->SPIR-V
//    compilation does not need GPU ray-tracing support).
//  - RayTracingPipelineBuildsOnHardware is GATED on a tier-2 device
//    (VK_KHR_ray_tracing_pipeline) and skips gracefully otherwise; it exercises
//    ray-tracing pipeline creation and the shader binding table build on real RT
//    hardware.
//
//  The inline GLSL mirrors shaders/rt/raytrace.{rgen,rmiss,rchit}.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/CommandBuffer.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

using namespace nexus::gfx;

namespace {

constexpr std::string_view kRaygen = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0, rgba8) uniform image2D outImage;
layout(location = 0) rayPayloadEXT vec3 hitValue;
void main() {
    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 uv = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    const vec3 origin = vec3(uv * 2.0 - 1.0, 1.0);
    const vec3 direction = vec3(0.0, 0.0, -1.0);
    hitValue = vec3(0.0);
    traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0,
                origin, 0.001, direction, 100.0, 0);
    imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(hitValue, 1.0));
}
)GLSL";

constexpr std::string_view kMiss = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadInEXT vec3 hitValue;
void main() { hitValue = vec3(0.05, 0.05, 0.08); }
)GLSL";

constexpr std::string_view kClosestHit = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;
void main() {
    hitValue = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
}
)GLSL";

// Raygen variant that writes into a storage buffer (SSBO) instead of an image,
// so results can be read back without an image->buffer copy.
constexpr std::string_view kRaygenSSBO = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) buffer OutBuf { vec4 pixels[]; };
layout(location = 0) rayPayloadEXT vec3 hitValue;
void main() {
    const uint idx = gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;
    const vec2 uv = (vec2(gl_LaunchIDEXT.xy) + 0.5) / vec2(gl_LaunchSizeEXT.xy);
    const vec3 origin = vec3(uv * 2.0 - 1.0, 1.0);
    const vec3 direction = vec3(0.0, 0.0, -1.0);
    hitValue = vec3(0.0);
    traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0,
                origin, 0.001, direction, 100.0, 0);
    pixels[idx] = vec4(hitValue, 1.0);
}
)GLSL";

std::unique_ptr<RenderContext> makeContext(bool enableRayTracing)
{
    RenderContextDesc desc{};
    desc.preferredBackend  = Backend::Vulkan;
    desc.validation        = ValidationLevel::Off;
    desc.enableMeshShaders = false;
    desc.enableRayTracing  = enableRayTracing;
    return RenderContext::create(desc);
}

struct RtShaders {
    ShaderHandle raygen;
    ShaderHandle miss;
    ShaderHandle closestHit;
};

RtShaders createRtShaders(IDevice& dev)
{
    ShaderDesc rg{}; rg.stage = ShaderStage::RayGen;     rg.glslSource = kRaygen;     rg.debugName = "rt.bringup.rgen";
    ShaderDesc ms{}; ms.stage = ShaderStage::Miss;       ms.glslSource = kMiss;       ms.debugName = "rt.bringup.rmiss";
    ShaderDesc ch{}; ch.stage = ShaderStage::ClosestHit; ch.glslSource = kClosestHit; ch.debugName = "rt.bringup.rchit";
    return { dev.createShader(rg), dev.createShader(ms), dev.createShader(ch) };
}

void destroyRtShaders(IDevice& dev, const RtShaders& s)
{
    if (s.raygen.valid())     dev.destroyShader(s.raygen);
    if (s.miss.valid())       dev.destroyShader(s.miss);
    if (s.closestHit.valid()) dev.destroyShader(s.closestHit);
}

} // namespace

TEST(VulkanRayTracingDispatch, ShadersCompileToModules)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = makeContext(/*enableRayTracing=*/false);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice& dev = ctx->device();
    ASSERT_EQ(dev.backend(), Backend::Vulkan);

    // GLSL->SPIR-V compilation and shader-module creation do not require the
    // device to support ray tracing — only well-formed RT SPIR-V.
    const RtShaders shaders = createRtShaders(dev);
    EXPECT_TRUE(shaders.raygen.valid());
    EXPECT_TRUE(shaders.miss.valid());
    EXPECT_TRUE(shaders.closestHit.valid());
    destroyRtShaders(dev, shaders);
}

TEST(VulkanRayTracingDispatch, RayTracingPipelineBuildsOnHardware)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = makeContext(/*enableRayTracing=*/true);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice& dev = ctx->device();

    if (dev.caps().rayTracingTier < 2) {
        GTEST_SKIP() << "device lacks VK_KHR_ray_tracing_pipeline (tier 2); "
                        "RT pipeline + SBT build cannot be exercised here";
    }

    const RtShaders shaders = createRtShaders(dev);
    ASSERT_TRUE(shaders.raygen.valid());
    ASSERT_TRUE(shaders.miss.valid());
    ASSERT_TRUE(shaders.closestHit.valid());

    const std::array<ShaderHandle, 1> miss{ shaders.miss };
    const std::array<ShaderHandle, 1> hit{ shaders.closestHit };

    RayTracingPipelineDesc rtd{};
    rtd.rayGenShader      = shaders.raygen;
    rtd.missShaders       = miss;
    rtd.closestHitShaders = hit;
    rtd.maxRecursionDepth = 1;
    rtd.debugName         = "rt.bringup.pipeline";

    // Creating the pipeline also builds its shader binding table; a valid handle
    // means group-handle query + SBT layout + buffer fill all succeeded on hardware.
    const PipelineHandle pso = dev.createRayTracingPipeline(rtd);
    EXPECT_TRUE(pso.valid());

    if (pso.valid()) dev.destroyPipeline(pso);
    destroyRtShaders(dev, shaders);
}

TEST(VulkanRayTracingDispatch, DispatchAndReadbackOnHardware)
{
    // Full bring-up: triangle BLAS/TLAS -> RT pipeline + SBT -> traceRays into an
    // SSBO -> readback -> assert a mix of hit (barycentric) and miss (background)
    // pixels. Gated on tier-2 RT hardware; skips on software/non-RT devices.
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = makeContext(/*enableRayTracing=*/true);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice& dev = ctx->device();
    if (dev.caps().rayTracingTier < 2) {
        GTEST_SKIP() << "device lacks VK_KHR_ray_tracing_pipeline (tier 2)";
    }

    constexpr uint32_t W = 16, H = 16;

    // 1) Triangle geometry: tightly-packed vec3 positions + uint32 indices.
    const float verts[] = { -0.4f, -0.4f, 0.0f,   0.4f, -0.4f, 0.0f,   0.0f, 0.4f, 0.0f };
    const uint32_t indices[] = { 0u, 1u, 2u };
    BufferDesc vbDesc{};
    vbDesc.sizeBytes = sizeof(verts);
    vbDesc.usage     = BufferUsage::RayTracingAS | BufferUsage::TransferDst;
    vbDesc.memory    = MemoryHint::GpuOnly;
    BufferDesc ibDesc{};
    ibDesc.sizeBytes = sizeof(indices);
    ibDesc.usage     = BufferUsage::RayTracingAS | BufferUsage::TransferDst;
    ibDesc.memory    = MemoryHint::GpuOnly;
    const BufferHandle vb = dev.createBuffer(vbDesc);
    const BufferHandle ib = dev.createBuffer(ibDesc);
    ASSERT_TRUE(vb.valid());
    ASSERT_TRUE(ib.valid());
    dev.uploadBuffer(vb, verts, sizeof(verts));
    dev.uploadBuffer(ib, indices, sizeof(indices));

    // 2) Acceleration structures.
    const AccelStructHandle blas = dev.buildBLAS(vb, ib, 3u, 3u);
    ASSERT_TRUE(blas.valid());
    const std::array<AccelStructHandle, 1> blases{ blas };
    const AccelStructHandle tlas = dev.buildTLAS(blases);
    ASSERT_TRUE(tlas.valid());

    // 3) Host-readable output SSBO (vec4 per pixel).
    const uint64_t outBytes = uint64_t(W) * H * 4u * sizeof(float);
    BufferDesc obDesc{};
    obDesc.sizeBytes = outBytes;
    obDesc.usage     = BufferUsage::StorageBuffer | BufferUsage::TransferSrc;
    obDesc.memory    = MemoryHint::GpuToCpu;
    const BufferHandle outBuf = dev.createBuffer(obDesc);
    ASSERT_TRUE(outBuf.valid());

    // 4) Descriptor layout for set 0: TLAS @0, output SSBO @1.
    std::array<DescriptorBindingDesc, 2> binds{};
    binds[0].binding = 0; binds[0].type = DescriptorType::AccelerationStructure; binds[0].accelStruct = tlas;
    binds[1].binding = 1; binds[1].type = DescriptorType::StorageBuffer;         binds[1].buffer = outBuf;

    // 5) RT pipeline (SSBO-output raygen) with a matching set-0 layout, builds SBT.
    ShaderDesc rg{}; rg.stage = ShaderStage::RayGen;     rg.glslSource = kRaygenSSBO;
    ShaderDesc ms{}; ms.stage = ShaderStage::Miss;       ms.glslSource = kMiss;
    ShaderDesc ch{}; ch.stage = ShaderStage::ClosestHit; ch.glslSource = kClosestHit;
    const ShaderHandle rgh = dev.createShader(rg);
    const ShaderHandle msh = dev.createShader(ms);
    const ShaderHandle chh = dev.createShader(ch);
    ASSERT_TRUE(rgh.valid() && msh.valid() && chh.valid());

    const std::array<ShaderHandle, 1> missArr{ msh };
    const std::array<ShaderHandle, 1> hitArr{ chh };
    RayTracingPipelineDesc rtd{};
    rtd.rayGenShader       = rgh;
    rtd.missShaders        = missArr;
    rtd.closestHitShaders  = hitArr;
    rtd.descriptorBindings = binds;
    rtd.maxRecursionDepth  = 1;
    const PipelineHandle pso = dev.createRayTracingPipeline(rtd);
    ASSERT_TRUE(pso.valid());

    // 6) Allocate + write the descriptor set (layout matches the pipeline's set 0).
    DescriptorSetDesc dsd{};
    dsd.bindings = binds;
    dsd.debugName = "rt.bringup.set";
    const DescriptorSetHandle set = dev.allocateDescriptorSet(dsd);
    ASSERT_TRUE(set.valid());
    dev.updateDescriptorSet(set, binds);

    // 7) Record + submit the trace.
    const CmdBufHandle cbh = dev.allocateCommandBuffer(QueueType::Graphics);
    ICommandBuffer* cmd = dev.getCommandBuffer(cbh);
    ASSERT_NE(cmd, nullptr);
    cmd->begin();
    cmd->bindPipeline(pso);          // activates the SBT
    cmd->bindDescriptorSet(set, 0);
    cmd->traceRays(W, H, 1);
    cmd->end();
    const FenceHandle fence = dev.createFence(false);
    const std::array<CmdBufHandle, 1> cmds{ cbh };
    dev.submit(QueueType::Graphics, cmds, {}, {}, fence);
    dev.waitForFence(fence);

    // 8) Readback + assert a mix of hit and miss pixels.
    std::vector<float> pixels(size_t(W) * H * 4u, -1.0f);
    dev.readbackBuffer(outBuf, pixels.data(), outBytes);
    uint32_t hits = 0, misses = 0;
    for (uint32_t i = 0; i < W * H; ++i) {
        const float sum = pixels[i*4+0] + pixels[i*4+1] + pixels[i*4+2];
        if (sum > 0.5f)       ++hits;    // barycentric colors sum to ~1.0
        else if (sum > 0.0f)  ++misses;  // background (0.05+0.05+0.08 = 0.18)
    }
    EXPECT_GT(hits, 0u);    // triangle hit -> BLAS/TLAS/SBT/closest-hit all worked
    EXPECT_GT(misses, 0u);  // some rays missed -> miss shader worked

    // 9) Cleanup.
    dev.destroyPipeline(pso);
    dev.freeDescriptorSet(set);
    dev.freeCommandBuffer(cbh);
    dev.destroyFence(fence);
    dev.destroyAccelStruct(tlas);
    dev.destroyAccelStruct(blas);
    dev.destroyBuffer(outBuf);
    dev.destroyBuffer(vb);
    dev.destroyBuffer(ib);
    dev.destroyShader(rgh);
    dev.destroyShader(msh);
    dev.destroyShader(chh);
}
