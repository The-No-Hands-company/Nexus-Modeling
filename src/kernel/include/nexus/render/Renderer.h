#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Renderer
//
//  Orchestrates per-frame rendering.  Selects rasterization vs path tracing
//  vs mesh-shader path based on HardwareTier and user preference.
//
//  Frame pipeline (high-end):
//    1. Frustum cull (CPU / GPU indirect)
//    2. Mesh shader geometry amplification (LOD / meshlets)
//    3. Rasterize GBuffer (or path-trace directly)
//    4. Async compute: denoising / up-scaling (DLSS / XeSS / OIDN)
//    5. TAA / temporal accumulation
//    6. Tone mapping → swapchain present
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/gfx/FrameScheduler.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/render/DescriptorBinder.h>
#include <nexus/render/ShadowMapTarget.h>
#include <nexus/render/FrameTimingLayer.h>
#include <nexus/render/RenderGraphValidator.h>
#include <nexus/render/FrameCaptureExporter.h>
#include <nexus/render/GaussianSplatPass.h>
#include <nexus/render/TemporalAccumulation.h>
#include <nexus/neural/NeuralRenderer.h>
#include <array>
#include <cstdint>
#include <memory>

namespace nexus::render {

// ── Screen-Space Ambient Occlusion settings ───────────────────────────────────
struct AOSettings {
    float    radius      = 0.5f;   // hemisphere sample radius in view space
    float    bias        = 0.025f; // depth bias to avoid self-occlusion
    uint32_t sampleCount = 16;     // hemisphere samples per pixel
    uint32_t blurPasses  = 2;      // bilateral blur passes after SSAO
};

// ── Screen-Space Reflection settings ─────────────────────────────────────────
struct SSRSettings {
    uint32_t maxRaySteps   = 64;    // max depth-buffer march steps per ray
    float    stepSize      = 0.1f;  // initial step size in view space
    float    thickness     = 0.05f; // depth thickness tolerance
    float    fadeDistance  = 10.f;  // fade-out distance from hit point
};

// ── Bloom settings ────────────────────────────────────────────────────────────
struct BloomSettings {
    float    threshold = 1.f;   // HDR luminance threshold for bloom source
    float    intensity = 0.04f; // bloom contribution scale at composite
    float    radius    = 0.85f; // Kawase blur radius scale
    uint32_t passes    = 5;     // downsample + upsample pass pairs
};

// ── Image-Based Lighting settings ────────────────────────────────────────────
struct IBLSettings {
    bool     enabled       = false;
    float    intensity     = 1.f;    // overall IBL contribution multiplier
    float    diffuseScale  = 1.f;    // irradiance map contribution scale
    float    specularScale = 1.f;    // prefiltered env map contribution scale
    uint32_t mipLevels     = 6;      // prefiltered mip levels to sample
};

// ── Order-Independent Transparency settings ───────────────────────────────────
struct OITSettings {
    bool     enabled     = false;
    uint32_t maxLayers   = 8;     // maximum transparent layers per pixel
    float    weightScale = 1.f;   // moment-based weight normalisation scale
};

// ── Depth of Field settings ───────────────────────────────────────────────────
struct DoFSettings {
    float    focalDistance = 10.f;   // world-space distance of focus plane
    float    focalRange    = 5.f;    // ±range around focalDistance that stays sharp
    float    maxCoC        = 0.05f;  // maximum circle-of-confusion radius (NDC)
    uint32_t sampleRadius  = 8;      // bokeh tap radius in pixels
};

// ── Motion Blur settings ──────────────────────────────────────────────────────
struct MotionBlurSettings {
    float    shutterAngle    = 180.f; // degrees; 360 = full-frame blur
    uint32_t sampleCount     = 8;    // velocity-vector integration samples
    float    maxBlurRadius   = 32.f; // maximum blur length in pixels
};

// ── Tone Mapping settings ─────────────────────────────────────────────────────
enum class ToneMappingOperator : uint8_t {
    Linear   = 0,  // passthrough (clamp only)
    Reinhard = 1,  // simple luminance Reinhard
    ACES     = 2,  // ACES filmic approximation
};

struct ToneMappingSettings {
    float               exposure    = 1.f;                      // linear pre-exposure multiplier
    float               whitePoint  = 1.f;                      // Reinhard white point
    ToneMappingOperator operator_   = ToneMappingOperator::ACES; // curve applied before present
};

// ── Volumetric lighting settings ─────────────────────────────────────────────
// Controls the froxel-based atmospheric scattering pass. When enabled, a
// compute dispatch integrates inscattering along view rays through a 3-D
// froxel grid before the composite pass.
struct VolumetricSettings {
    bool    enabled                  = false;
    float   fogDensity               = 0.02f;   // global extinction scale
    float   scatteringCoeff          = 0.5f;    // single-scatter albedo (0–1)
    float   extinctionCoeff          = 0.02f;   // per-unit extinction
    uint32_t froxelSlices            = 64;      // depth slices in froxel grid
    uint32_t froxelResolutionDivisor = 8;       // froxel XY = render target / divisor
};

// ── Render mode ───────────────────────────────────────────────────────────────
enum class RenderMode : uint8_t {
    Rasterize,        // classic rasterisation (all hardware tiers)
    PathTrace,        // hardware RT path tracing (High / Ultra only)
    HybridRT,         // raster primary rays + RT secondary (Mid+)
    Wireframe,        // debug: mesh edges only
    Normals,          // debug: world-space normals
    Overdraw,         // debug: fragment overdraw heat-map
};

// ── Up-scaling mode ───────────────────────────────────────────────────────────
enum class UpscaleMode : uint8_t {
    Off,
    DLSS4,    // NVIDIA
    XeSS,     // Intel
    FSR3,     // AMD
    Bilinear, // software fallback
};

// ── Renderer settings ─────────────────────────────────────────────────────────
struct RendererSettings {
    RenderMode  mode            = RenderMode::Rasterize;
    UpscaleMode upscale         = UpscaleMode::Off;
    float       renderScale     = 1.0f;  // <1.0 = render at lower res, then upscale
    uint32_t    maxBounces      = 4;     // path tracing bounce limit
    bool        enableTAA       = true;
    bool        enableShadows   = true;
    bool        enableAO        = true;
    bool        enableBloom     = true;
    bool        enableSSR       = false; // screen-space reflections
    bool        enableRTReflect   = false; // RT reflections (High+ tier)
    bool        enableMeshShaders = true;  // use mesh shader path when caps allow
    bool        enableDenoising   = false; // route post-composite pass through INeuralRenderer
    bool        enableUpscaling   = false; // route post-denoise pass through INeuralRenderer::upscale
    bool        enableVRS         = false; // variable-rate shading (opt-in; ignored when caps().variableRateShading == false)
    nexus::gfx::ShadingRate defaultShadingRate = nexus::gfx::ShadingRate::Rate2x2; // coarse rate applied to non-focused regions
    uint8_t     msaaSamples       = 1;    // MSAA sample count; clamped to caps().maxMsaaSamples; 1 = off
    bool        enableDoF           = false; // depth-of-field bokeh blur (post-composite)
    bool        enableMotionBlur    = false; // per-pixel velocity motion blur
    bool        enableToneMapping   = false; // HDR tone mapping before present
    bool        enableIBL           = false; // image-based lighting from environment map
    bool        enableOIT           = false; // order-independent transparency resolve pass
};

// ── Per-frame stats ───────────────────────────────────────────────────────────
struct FrameStats {
    uint32_t totalNodes       = 0;
    uint32_t visibleNodes     = 0;
    uint32_t drawCalls            = 0;
    uint32_t shadowDrawCalls      = 0;  // shadow-pass draw calls only (subset of drawCalls)
    uint32_t meshShaderDrawCalls  = 0;  // draw calls dispatched via drawMeshTasks this frame
    uint32_t triangles            = 0;
    uint32_t meshlets             = 0;
    // Gaussian splat pass contributions (additive; zero when no pass attached).
    uint32_t splatDrawCalls   = 0;
    uint32_t submittedSplats  = 0;
    uint32_t projectedSplats  = 0;
    double   gpuTimeMs        = 0.0;
    double   cpuCullTimeMs    = 0.0;
    uint32_t taaFrameIndex    = 0;   // TemporalAccumulator frame counter (0 when TAA off)
    bool     denoisingActive  = false;                                  // true when denoiser ran this frame
    nexus::neural::DenoiserBackend activeDenoiser =
        nexus::neural::DenoiserBackend::None;                           // backend used this frame
    bool     upscalingActive  = false;                                  // true when upscaler ran this frame
    nexus::neural::UpscalerBackend activeUpscaler =
        nexus::neural::UpscalerBackend::None;                           // backend used this frame
    RenderMode activeRenderMode  = RenderMode::Rasterize;              // mode used after capability downgrade
    bool     rtReflectionsActive = false;                              // true when RT reflection pass ran
    uint32_t tlasInstanceCount   = 0;                                  // BLAS instances in scene TLAS this frame
    bool     vrsActive            = false;                             // true when VRS setFragmentShadingRate fired this frame
    bool     volumetricActive     = false;                             // true when froxel compute pass ran
    uint32_t volumetricFroxelCount = 0;                               // total froxels dispatched (w * h * slices)
    uint8_t  msaaSamples          = 1;                                // MSAA sample count actually used this frame
    bool     aoActive             = false;                            // true when SSAO compute pass ran
    uint32_t aoSampleCount        = 0;                                // hemisphere samples used (pixels × sampleCount)
    bool     ssrActive            = false;                            // true when SSR compute pass ran
    uint32_t ssrRayCount          = 0;                                // total SSR rays dispatched this frame
    bool     bloomActive          = false;                            // true when bloom pass chain ran
    uint32_t bloomPassCount       = 0;                                // downsample+upsample passes fired this frame
    bool     dofActive            = false;                            // true when DoF compute pass ran
    uint32_t dofSampleCount       = 0;                                // CoC samples used this frame (pixels × sampleRadius)
    bool     motionBlurActive     = false;                            // true when motion blur pass ran
    uint32_t motionBlurSampleCount = 0;                               // velocity samples integrated this frame
    bool     tonemapActive        = false;                            // true when tonemap pass ran
    ToneMappingOperator tonemapOperator = ToneMappingOperator::Linear; // operator used this frame
    bool     iblActive            = false;                            // true when IBL compute dispatch ran
    uint32_t iblMipLevels         = 0;                                // prefiltered mip levels sampled this frame
    bool     oitActive            = false;                            // true when OIT resolve pass ran
    uint32_t oitFragmentCount     = 0;                                // transparent fragments accumulated
};

// ── Composite input diagnostic ────────────────────────────────────────────────
// Named flags for each reason a composite or shadow input may be absent.
// Multiple flags may be set simultaneously; None means all required fields present.
enum class CompositeInputMissing : uint32_t {
    None                 = 0,
    GBufferAlbedo        = 1 << 0,
    GBufferNormal        = 1 << 1,
    GBufferVelocity      = 1 << 2,
    GBufferDepth         = 1 << 3,
    SamplerAlbedo        = 1 << 4,
    SamplerNormal        = 1 << 5,
    SamplerVelocity      = 1 << 6,
    SamplerDepth         = 1 << 7,
    MaterialTable        = 1 << 8,  // only flagged when materialTableOffsetBytes > 0
    ShadowDepthTexture   = 1 << 9,
    ShadowDepthSampler   = 1 << 10,
    ShadowLightingBuffer = 1 << 11,
    ShadowCascadeCount   = 1 << 12,
};
inline CompositeInputMissing operator|(CompositeInputMissing a, CompositeInputMissing b) noexcept {
    return static_cast<CompositeInputMissing>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline CompositeInputMissing operator&(CompositeInputMissing a, CompositeInputMissing b) noexcept {
    return static_cast<CompositeInputMissing>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool hasFlag(CompositeInputMissing val, CompositeInputMissing flag) noexcept {
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

struct CompositeMaterialBindings {
    nexus::gfx::SamplerHandle albedoMaterialSampler;
    nexus::gfx::SamplerHandle normalSampler;
    nexus::gfx::SamplerHandle velocitySampler;
    nexus::gfx::SamplerHandle depthSampler;
    nexus::gfx::SamplerHandle shadowDepthSampler;
    nexus::gfx::BufferHandle  materialTable;
    uint32_t                  materialTableOffsetBytes = 0;
};

struct CompositePassBindingDesc {
    nexus::gfx::TextureHandle albedoTexture;
    nexus::gfx::TextureHandle normalTexture;
    nexus::gfx::TextureHandle velocityTexture;
    nexus::gfx::TextureHandle depthTexture;
    nexus::gfx::SamplerHandle albedoSampler;
    nexus::gfx::SamplerHandle normalSampler;
    nexus::gfx::SamplerHandle velocitySampler;
    nexus::gfx::SamplerHandle depthSampler;
    nexus::gfx::BufferHandle  materialTable;
    uint32_t                  materialTableOffsetBytes = 0;
    nexus::gfx::TextureHandle shadowDepthTexture;
    nexus::gfx::SamplerHandle shadowDepthSampler;
    nexus::gfx::BufferHandle  shadowLightingBuffer;
    uint32_t                  shadowCascadeCount = 0;

    [[nodiscard]] bool hasCoreInputs() const noexcept
    {
        return albedoTexture.valid()
            && normalTexture.valid()
            && velocityTexture.valid()
            && depthTexture.valid()
            && albedoSampler.valid()
            && normalSampler.valid()
            && velocitySampler.valid()
            && depthSampler.valid();
    }

    [[nodiscard]] bool hasMaterialTableInput() const noexcept
    {
        return materialTable.valid();
    }

    [[nodiscard]] bool hasShadowInputs() const noexcept
    {
        return shadowDepthTexture.valid()
            && shadowDepthSampler.valid()
            && shadowLightingBuffer.valid()
            && shadowCascadeCount > 0;
    }

    [[nodiscard]] bool readyForComposite() const noexcept
    {
        return hasCoreInputs()
            && (materialTableOffsetBytes == 0 || hasMaterialTableInput());
    }

    // Returns a bitmask of every field that is absent or invalid.
    // None means all fields checked by this call are present; the composite pass
    // is safe to run when the returned value contains no flags that block it
    // (i.e. GBuffer*, Sampler*, and MaterialTable when offset is set).
    [[nodiscard]] CompositeInputMissing diagnose() const noexcept
    {
        CompositeInputMissing missing = CompositeInputMissing::None;
        if (!albedoTexture.valid())
            missing = missing | CompositeInputMissing::GBufferAlbedo;
        if (!normalTexture.valid())
            missing = missing | CompositeInputMissing::GBufferNormal;
        if (!velocityTexture.valid())
            missing = missing | CompositeInputMissing::GBufferVelocity;
        if (!depthTexture.valid())
            missing = missing | CompositeInputMissing::GBufferDepth;
        if (!albedoSampler.valid())
            missing = missing | CompositeInputMissing::SamplerAlbedo;
        if (!normalSampler.valid())
            missing = missing | CompositeInputMissing::SamplerNormal;
        if (!velocitySampler.valid())
            missing = missing | CompositeInputMissing::SamplerVelocity;
        if (!depthSampler.valid())
            missing = missing | CompositeInputMissing::SamplerDepth;
        if (materialTableOffsetBytes > 0 && !materialTable.valid())
            missing = missing | CompositeInputMissing::MaterialTable;
        if (!shadowDepthTexture.valid())
            missing = missing | CompositeInputMissing::ShadowDepthTexture;
        if (!shadowDepthSampler.valid())
            missing = missing | CompositeInputMissing::ShadowDepthSampler;
        if (!shadowLightingBuffer.valid())
            missing = missing | CompositeInputMissing::ShadowLightingBuffer;
        if (shadowCascadeCount == 0)
            missing = missing | CompositeInputMissing::ShadowCascadeCount;
        return missing;
    }
};

struct ShadowLightingContract {
    static constexpr uint32_t kMaxCascades = 4;
    Mat4     lightViewProj[kMaxCascades]{};
    float    cascadeSplits[kMaxCascades]{};
    uint32_t cascadeCount = 0;
    float    shadowBias = 0.0005f;
    float    normalBias = 0.001f;
};

struct ShadowCascadePassDesc {
    uint32_t            cascadeIndex = 0;
    nexus::gfx::Rect2D  atlasRect{};
    uint32_t            drawCalls = 0;
    uint32_t            triangles = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return atlasRect.extent.width > 0 && atlasRect.extent.height > 0;
    }
};

// ── Shadow pass chain descriptor ─────────────────────────────────────────────
// Exposes the owned shadow atlas and per-cascade pass descriptors so tests and
// backend binding code can validate atlas lifetime, cascade packing, and
// sampling readiness independently of composite inputs.
struct ShadowPassBindingDesc {
    static constexpr uint32_t kMaxCascades = ShadowLightingContract::kMaxCascades;

    nexus::gfx::TextureHandle                  atlasDepthTexture;
    nexus::gfx::Extent2D                       atlasExtent{};
    nexus::gfx::Extent2D                       cascadeExtent{};
    nexus::gfx::TextureLayout                  atlasDepthLayout = nexus::gfx::TextureLayout::Undefined;
    uint32_t                  cascadeCount = 0;
    uint32_t                  atlasColumns = 0;
    uint32_t                  atlasRows = 0;
    std::array<ShadowCascadePassDesc, kMaxCascades> cascadePasses{};

    [[nodiscard]] bool hasAtlasTarget() const noexcept
    {
        return atlasDepthTexture.valid() && atlasExtent.width > 0 && atlasExtent.height > 0;
    }

    [[nodiscard]] bool hasDepthTarget() const noexcept
    {
        return hasAtlasTarget();
    }

    [[nodiscard]] bool hasPassDescriptors() const noexcept
    {
        if (!hasAtlasTarget() || cascadeCount == 0 || cascadeCount > kMaxCascades) {
            return false;
        }
        for (uint32_t i = 0; i < cascadeCount; ++i) {
            if (!cascadePasses[i].isValid() || cascadePasses[i].cascadeIndex != i) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool readyForDepthPass() const noexcept
    {
        return hasPassDescriptors();
    }

    [[nodiscard]] bool readyForLightingSampling() const noexcept
    {
        return hasPassDescriptors()
            && atlasDepthLayout == nexus::gfx::TextureLayout::DepthRead;
    }
};

// ── Dedicated shadow lighting binding descriptor ───────────────────────────────
// Carries all shadow-specific fields needed to bind a shadow lighting descriptor
// set, including per-cascade split values and bias parameters from the contract.
// Populated by Renderer::buildShadowLightingBindingDesc() and validated
// independently of the composite GBuffer inputs.
struct ShadowLightingBindingDesc {
    static constexpr uint32_t kMaxCascades = ShadowLightingContract::kMaxCascades;

    nexus::gfx::TextureHandle shadowDepthTexture;
    nexus::gfx::SamplerHandle shadowDepthSampler;
    nexus::gfx::BufferHandle  shadowLightingBuffer;
    uint32_t                  shadowCascadeCount = 0;
    Mat4                      lightViewProj[kMaxCascades]{};
    float                     cascadeSplits[kMaxCascades]{};
    float                     shadowBias = 0.0f;
    float                     normalBias = 0.0f;

    // All four required fields are valid/non-zero.
    [[nodiscard]] bool isComplete() const noexcept
    {
        return shadowDepthTexture.valid()
            && shadowDepthSampler.valid()
            && shadowLightingBuffer.valid()
            && shadowCascadeCount > 0;
    }
};

// ── Renderer ──────────────────────────────────────────────────────────────────
class Renderer {
public:
    explicit Renderer(nexus::gfx::RenderContext& ctx, nexus::gfx::ISwapchain& swapchain);
    // Optional: wire an IFrameScheduler for production Vulkan rendering.
    // When set, beginFrame/endFrame delegate to the scheduler.
    void setFrameScheduler(nexus::gfx::IFrameScheduler* scheduler) noexcept;
    ~Renderer();

    // ── Per-frame entry points ─────────────────────────────────────────────
    void beginFrame();
    void render(const Camera& camera, SceneGraph& scene);
    void endFrame();

    // ── Configuration ──────────────────────────────────────────────────────
    void applySettings(const RendererSettings& s);
    void setFallbackGeometryPipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    void setFallbackMeshPipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    // RT pipeline used when mode == PathTrace or mode == HybridRT (capability-gated).
    // Ignored on hardware without rayTracingTier >= 1; no-op on Null backend.
    void setRayTracingPipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    [[nodiscard]] nexus::gfx::PipelineHandle rayTracingPipeline() const noexcept;

    // TLAS bound as set=0 binding=0 in the RT descriptor layout when RT dispatch fires.
    // Non-owning: caller is responsible for the handle lifetime. Null handle disables
    // the descriptor bind (RT dispatch still fires when other gate conditions hold).
    void setSceneTLAS(nexus::gfx::AccelStructHandle tlas) noexcept;
    [[nodiscard]] nexus::gfx::AccelStructHandle sceneTLAS() const noexcept;

    // SBT used for RT dispatch. When valid, traceRaysWithSBT is used instead of
    // the bare traceRays call, enabling per-material hit/miss shader selection.
    // Non-owning: caller is responsible for the handle lifetime.
    void setShaderBindingTable(nexus::gfx::SBTHandle sbt) noexcept;
    [[nodiscard]] nexus::gfx::SBTHandle shaderBindingTable() const noexcept;

    // Volumetric lighting pass configuration.
    void setVolumetricSettings(const VolumetricSettings& settings) noexcept;
    [[nodiscard]] const VolumetricSettings& volumetricSettings() const noexcept;

    // Screen-space post-processing pass configuration.
    void setAOSettings(const AOSettings& settings) noexcept;
    [[nodiscard]] const AOSettings& aoSettings() const noexcept;

    void setSSRSettings(const SSRSettings& settings) noexcept;
    [[nodiscard]] const SSRSettings& ssrSettings() const noexcept;

    void setBloomSettings(const BloomSettings& settings) noexcept;
    [[nodiscard]] const BloomSettings& bloomSettings() const noexcept;

    void setDoFSettings(const DoFSettings& settings) noexcept;
    [[nodiscard]] const DoFSettings& dofSettings() const noexcept;

    void setMotionBlurSettings(const MotionBlurSettings& settings) noexcept;
    [[nodiscard]] const MotionBlurSettings& motionBlurSettings() const noexcept;

    void setToneMappingSettings(const ToneMappingSettings& settings) noexcept;
    [[nodiscard]] const ToneMappingSettings& toneMappingSettings() const noexcept;

    void setIBLSettings(const IBLSettings& settings) noexcept;
    [[nodiscard]] const IBLSettings& iblSettings() const noexcept;

    void setOITSettings(const OITSettings& settings) noexcept;
    [[nodiscard]] const OITSettings& oitSettings() const noexcept;

    void setShadowPipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    void setShadowMeshPipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    void setLightingCompositePipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    void setMaterialPipeline(MaterialID material, nexus::gfx::PipelineHandle pipeline) noexcept;
    void setMaterialMeshPipeline(MaterialID material, nexus::gfx::PipelineHandle pipeline) noexcept;
    void clearMaterialPipelines() noexcept;
    void clearMaterialMeshPipelines() noexcept;
    void setCompositeMaterialBindings(const CompositeMaterialBindings& bindings) noexcept;
    void clearCompositeMaterialBindings() noexcept;
    void setShadowLightingContract(const ShadowLightingContract& contract) noexcept;
    void clearShadowLightingContract() noexcept;
    [[nodiscard]] ShadowPassBindingDesc     buildShadowPassBindingDesc()      const noexcept;
    [[nodiscard]] CompositePassBindingDesc   buildCompositePassBindingDesc()   const noexcept;
    [[nodiscard]] ShadowLightingBindingDesc  buildShadowLightingBindingDesc()  const noexcept;
    void resetScene(SceneGraph& scene);
    void resetSceneAndDestroyTLAS(SceneGraph& scene);
    [[nodiscard]] const RendererSettings& settings() const noexcept { return m_settings; }

    // ── Render graph validation hook ───────────────────────────────────────
    // When set, Renderer::render() will validate and store the last frame's
    // render graph report.  Pass nullptr to disable.
    void setRenderGraphValidationEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isRenderGraphValidationEnabled() const noexcept;
    [[nodiscard]] const RenderGraphValidationReport& lastRenderGraphReport() const noexcept;

    // ── Frame capture hook ────────────────────────────────────────────────
    // Optional exporter called from beginFrame / recordPass / endFrame.
    // Pass nullptr to detach.
    void setFrameCaptureExporter(IFrameCaptureExporter* exporter) noexcept;
    [[nodiscard]] IFrameCaptureExporter* frameCaptureExporter() const noexcept;

    // ── Gaussian splat pass hook ─────────────────────────────────────────
    // Optional pass driven from render() after composite. The Renderer does
    // not take ownership; pass nullptr to detach. When set, the renderer
    // refreshes the pass camera matrices from the active Camera each frame,
    // invokes computeStats(), and accumulates the result into FrameStats.
    void setGaussianSplatPass(GaussianSplatPass* pass) noexcept;
    [[nodiscard]] GaussianSplatPass* gaussianSplatPass() const noexcept;

    // ── Descriptor binding integration (Month 14 Track 1) ────────────────
    // Builds a CompositeDescriptorSet from the current composite pass binding
    // desc (GBuffer textures + samplers + optional material table).  The set
    // is allocated on the first call and updated on subsequent calls.
    // Returns false when the current binding desc lacks the core GBuffer
    // inputs needed for allocation.
    [[nodiscard]] bool bindCompositeDescriptors(nexus::gfx::IDevice& dev);
    [[nodiscard]] const CompositeDescriptorSet* compositeDescriptorSet() const noexcept;
    void destroyCompositeDescriptors(nexus::gfx::IDevice& dev) noexcept;

    // ── Shadow descriptor set integration (Month 15 Track 1) ────────────
    // Builds a ShadowDescriptorSet from the current shadow lighting binding
    // desc.  Returns false when the shadow inputs are absent or incomplete.
    [[nodiscard]] bool bindShadowDescriptors(nexus::gfx::IDevice& dev);
    [[nodiscard]] const ShadowDescriptorSet* shadowDescriptorSet() const noexcept;
    void destroyShadowDescriptors(nexus::gfx::IDevice& dev) noexcept;

    // ── Shadow map target integration (Month 14 Track 2) ─────────────────
    // Attaches an externally-owned ShadowMapTarget to the renderer.
    // When attached, onResize() will call shadowMapTarget->resize() so the
    // depth atlas stays in sync with swapchain extent changes.
    // Pass nullptr to detach.  The renderer does not take ownership.
    void setShadowMapTarget(ShadowMapTarget* target) noexcept;
    [[nodiscard]] ShadowMapTarget* shadowMapTarget() const noexcept;

    // ── GPU timing layer (Month 16 Track 2) ──────────────────────────────
    // Enables per-pass GPU timestamp readback into FrameStats::gpuTimeMs.
    // Call once after setFrameScheduler() to allocate one QueryPool per slot.
    // Must be destroyed explicitly before the renderer is destroyed.
    [[nodiscard]] bool enableFrameTiming(uint32_t framesInFlight = 2);
    void disableFrameTiming() noexcept;
    [[nodiscard]] bool isFrameTimingEnabled() const noexcept;
    [[nodiscard]] const FrameTimingResult& lastFrameTimingResult() const noexcept;

    // ── Neural denoiser / upscaler (async-compute scheduling) ────────────
    // Non-owning. Pass nullptr to detach. Renderer calls denoise() after the
    // composite pass when enableDenoising is true and a renderer is attached.
    void setNeuralRenderer(nexus::neural::INeuralRenderer* nr) noexcept;
    [[nodiscard]] nexus::neural::INeuralRenderer* neuralRenderer() const noexcept;

    // ── Temporal anti-aliasing (TAA) ───────────────────────────────────────
    void setTemporalAccumulationConfig(const TemporalAccumulationConfig& cfg) noexcept;
    [[nodiscard]] const TemporalAccumulationConfig& temporalAccumulationConfig() const noexcept;
    [[nodiscard]] const TemporalAccumulationState&  temporalAccumulationState()  const noexcept;

    // ── Stats ──────────────────────────────────────────────────────────────
    [[nodiscard]] const FrameStats& lastFrameStats() const noexcept { return m_stats; }

    // ── Resize ────────────────────────────────────────────────────────────
    void onResize(nexus::gfx::Extent2D newExtent);

private:
    void selectRenderPath();   // updates m_activePath based on settings + caps
    void ensureGBuffer(nexus::gfx::Extent2D extent);
    void destroyGBuffer();
    void ensureShadowTargets(nexus::gfx::Extent2D extent, uint32_t cascadeCount);
    void destroyShadowTargets();
    void uploadShadowLightingContract();

    nexus::gfx::RenderContext& m_ctx;
    nexus::gfx::ISwapchain&    m_swapchain;
    RendererSettings           m_settings;
    FrameStats                 m_stats;

    CompositeDescriptorSet     m_compositeDescSet;
    ShadowDescriptorSet        m_shadowDescSet;
    ShadowMapTarget*           m_shadowMapTarget = nullptr;
    FrameTimingLayer           m_frameTiming;
    FrameTimingResult          m_lastTimingResult{};
    TemporalAccumulator        m_taaAccumulator;
    nexus::neural::INeuralRenderer* m_neuralRenderer = nullptr;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace nexus::render
