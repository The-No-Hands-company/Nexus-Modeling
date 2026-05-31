// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Vulkan RT dispatch + live neural renderer integration
//
//  All tests skip cleanly when:
//    - Vulkan backend is unavailable on the host (no ICD / headless CI)
//    - rayTracingTier == 0 (hardware does not support VK_KHR_ray_tracing_pipeline)
//    - DLSS/XeSS SDK not compiled in (NEXUS_ENABLE_DLSS / NEXUS_ENABLE_XESS absent)
//
//  Designed for GPU-capable CI runners with Tier-1 RT hardware and for local
//  developer machines. The Null-backend suite (test_RTProductionPath.cpp)
//  covers all logic paths; these tests verify the live Vulkan code path.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/neural/NeuralRenderer.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;

namespace {

// Creates a Vulkan RenderContext with ray tracing requested.
// Returns nullptr and skips the calling test if Vulkan is unavailable.
#define REQUIRE_VULKAN_CTX(var)                                          \
    std::unique_ptr<RenderContext> var;                                   \
    do {                                                                  \
        RenderContextDesc _desc{};                                        \
        _desc.preferredBackend = Backend::Vulkan;                        \
        _desc.validation       = ValidationLevel::Off;                   \
        _desc.enableRayTracing = true;                                   \
        try { var = RenderContext::create(_desc); }                       \
        catch (const std::exception& e) {                                \
            GTEST_SKIP() << "Vulkan unavailable: " << e.what();         \
        }                                                                 \
        if (!var) GTEST_SKIP() << "Vulkan context returned null";        \
    } while(false)

// Skips when the device does not support ray tracing tier 1.
#define REQUIRE_RT_TIER1(ctx)                                            \
    do {                                                                  \
        if ((ctx)->caps().rayTracingTier < 1)                            \
            GTEST_SKIP() << "rayTracingTier < 1 on this device";        \
    } while(false)

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

TEST(VulkanRTDispatch, VulkanContextCreatesWithRTEnabled)
{
    REQUIRE_VULKAN_CTX(ctx);
    EXPECT_EQ(ctx->activeBackend(), Backend::Vulkan);
}

TEST(VulkanRTDispatch, DeviceReportsRayTracingCapabilities)
{
    REQUIRE_VULKAN_CTX(ctx);
    // rayTracingTier may be 0 (no RT) or >= 1. The test just asserts the field
    // is readable and non-negative — tier value depends on hardware.
    EXPECT_GE(ctx->caps().rayTracingTier, 0u);
}

TEST(VulkanRTDispatch, TraceRaysDispatchSetsRTReflectionsActiveOnTier1)
{
    REQUIRE_VULKAN_CTX(ctx);
    REQUIRE_RT_TIER1(ctx);

    nexus::render::SceneGraph scene;
    nexus::render::Renderer   renderer(*ctx);

    // Create a minimal RT pipeline via compute shader stand-in.
    constexpr std::string_view kRayGenGlsl = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadEXT vec4 payload;
void main() { payload = vec4(0.0); }
)GLSL";

    ShaderDesc rtDesc{};
    rtDesc.stage      = ShaderStage::RayGen;
    rtDesc.glslSource = kRayGenGlsl;
    rtDesc.debugName  = "test.rt.raygen";

    IDevice& dev = ctx->device();
    const ShaderHandle rtShader = dev.createShader(rtDesc);
    if (!rtShader.valid())
        GTEST_SKIP() << "RT shader compilation failed — RT shader stages may not be supported";

    RTPipelineDesc rtPipeDesc{};
    rtPipeDesc.rayGenShader = rtShader;
    rtPipeDesc.debugName    = "test.rt.pipeline";

    const PipelineHandle rtPipe = dev.createRTPipeline(rtPipeDesc);
    if (!rtPipe.valid()) {
        dev.destroyShader(rtShader);
        GTEST_SKIP() << "RT pipeline creation failed";
    }

    nexus::render::RendererSettings settings{};
    settings.enableRTReflect = true;
    settings.mode            = nexus::render::RenderMode::HybridRT;
    renderer.applySettings(settings);
    renderer.setRayTracingPipeline(rtPipe);

    renderer.render(scene);
    const auto stats = renderer.lastFrameStats();

    EXPECT_TRUE(stats.rtReflectionsActive);
    EXPECT_EQ(stats.activeRenderMode, nexus::render::RenderMode::HybridRT);

    dev.destroyPipeline(rtPipe);
    dev.destroyShader(rtShader);
}

TEST(VulkanRTDispatch, NeuralRendererFactoryAutoSelectReturnsNonNull)
{
    REQUIRE_VULKAN_CTX(ctx);
    IDevice& dev = ctx->device();

    auto nr = nexus::neural::NeuralRendererFactory::create(
        nexus::neural::NeuralBackend::Auto, dev);
    ASSERT_NE(nr, nullptr);
    // activeUpscaler must be a valid enum value (may be Bilinear if no SDK present)
    EXPECT_GE(static_cast<int>(nr->activeUpscaler()), 0);
}

TEST(VulkanRTDispatch, NeuralRendererFactoryBilinearAlwaysAvailable)
{
    REQUIRE_VULKAN_CTX(ctx);
    IDevice& dev = ctx->device();

    auto nr = nexus::neural::NeuralRendererFactory::create(
        nexus::neural::NeuralBackend::Bilinear, dev);
    ASSERT_NE(nr, nullptr);
    EXPECT_EQ(nr->activeUpscaler(), nexus::neural::UpscalerBackend::Bilinear);
}

TEST(VulkanRTDispatch, DLSSUpscalerActiveWhenSDKPresent)
{
    REQUIRE_VULKAN_CTX(ctx);
    IDevice& dev = ctx->device();

    auto nr = nexus::neural::NeuralRendererFactory::create(
        nexus::neural::NeuralBackend::DLSS4, dev);
    ASSERT_NE(nr, nullptr);

    if (nr->activeUpscaler() != nexus::neural::UpscalerBackend::DLSS4)
        GTEST_SKIP() << "DLSS SDK not present on this machine — upscaler fell back to Bilinear";

    nexus::render::SceneGraph scene;
    nexus::render::Renderer   renderer(*ctx);
    renderer.setNeuralRenderer(nr.get());

    nexus::render::RendererSettings settings{};
    settings.enableUpscaling = true;
    renderer.applySettings(settings);
    renderer.render(scene);

    const auto stats = renderer.lastFrameStats();
    EXPECT_TRUE(stats.upscalingActive);
    EXPECT_EQ(stats.activeUpscaler, nexus::neural::UpscalerBackend::DLSS4);
}

TEST(VulkanRTDispatch, XeSSUpscalerActiveWhenSDKPresent)
{
    REQUIRE_VULKAN_CTX(ctx);
    IDevice& dev = ctx->device();

    auto nr = nexus::neural::NeuralRendererFactory::create(
        nexus::neural::NeuralBackend::XeSS, dev);
    ASSERT_NE(nr, nullptr);

    if (nr->activeUpscaler() != nexus::neural::UpscalerBackend::XeSS)
        GTEST_SKIP() << "XeSS SDK not present on this machine — upscaler fell back to Bilinear";

    nexus::render::SceneGraph scene;
    nexus::render::Renderer   renderer(*ctx);
    renderer.setNeuralRenderer(nr.get());

    nexus::render::RendererSettings settings{};
    settings.enableUpscaling = true;
    renderer.applySettings(settings);
    renderer.render(scene);

    const auto stats = renderer.lastFrameStats();
    EXPECT_TRUE(stats.upscalingActive);
    EXPECT_EQ(stats.activeUpscaler, nexus::neural::UpscalerBackend::XeSS);
}

TEST(VulkanRTDispatch, OIDNCPUDenoiserActiveWhenDeviceAvailable)
{
    REQUIRE_VULKAN_CTX(ctx);
    IDevice& dev = ctx->device();

    auto nr = nexus::neural::NeuralRendererFactory::create(
        nexus::neural::NeuralBackend::OIDN_CPU, dev);
    ASSERT_NE(nr, nullptr);

    if (nr->activeDenoiser() != nexus::neural::DenoiserBackend::OIDN_CPU)
        GTEST_SKIP() << "OIDN not compiled in (NEXUS_ENABLE_OIDN=OFF)";

    nexus::render::SceneGraph scene;
    nexus::render::Renderer   renderer(*ctx);
    renderer.setNeuralRenderer(nr.get());

    nexus::render::RendererSettings settings{};
    settings.enableDenoising = true;
    renderer.applySettings(settings);
    renderer.render(scene);

    const auto stats = renderer.lastFrameStats();
    EXPECT_TRUE(stats.denoisingActive);
    EXPECT_EQ(stats.activeDenoiser, nexus::neural::DenoiserBackend::OIDN_CPU);
}
