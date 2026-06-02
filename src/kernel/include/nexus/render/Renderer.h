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

// ── Subsurface Scattering settings ───────────────────────────────────────────
struct SSSSettings {
    bool     enabled      = false;
    float    scatterRadius = 1.f;   // world-space scatter kernel radius
    uint32_t profileCount  = 1;     // number of distinct SSS profiles in use
    uint32_t blurPasses    = 2;     // separable blur passes (horizontal + vertical per pair)
};

// ── Contact Shadow settings ───────────────────────────────────────────────────
struct ContactShadowSettings {
    bool     enabled     = false;
    float    rayLength   = 0.5f;    // world-space ray march length
    uint32_t sampleCount = 16;      // depth-buffer samples per ray
    float    thickness   = 0.05f;   // depth thickness bias
};

// ── Tiled Deferred Lighting settings ─────────────────────────────────────────
struct TiledLightingSettings {
    bool     enabled          = false;
    uint32_t tileSize         = 16;   // pixel tile side length (must be power of 2)
    uint32_t maxLightsPerTile = 256;  // light list capacity per tile
};

// ── Ray-Traced Global Illumination (RTGI) settings ───────────────────────────
struct RTGISettings {
    bool     enabled              = false;
    uint32_t raysPerPixel         = 2;     // RT rays dispatched per pixel
    uint32_t maxBounces           = 1;     // indirect bounce depth (1 = one-bounce GI)
    bool     denoised             = true;  // run screen-space denoiser pass after gather
    float    multiBounceFeedback  = 0.5f;  // prev-frame irradiance blend weight per bounce [0,1]
    float    temporalAlpha        = 0.1f;  // exponential blend weight for temporal reprojection
};

// ── Horizon-Based Ambient Occlusion (HBAO+) settings ─────────────────────────
struct HBAOSettings {
    bool     enabled     = false;
    float    radius      = 0.5f;   // world-space AO sampling radius
    float    angleBias   = 0.1f;   // minimum horizon angle bias to avoid self-occlusion
    uint32_t sliceCount  = 4;      // number of azimuth slices per pixel
    uint32_t stepCount   = 4;      // march steps per slice
};

// ── Variance Shadow Map (VSM) settings ────────────────────────────────────────
struct VSMSettings {
    bool     enabled              = false;
    float    blurRadius           = 2.f;    // Gaussian blur radius in texels
    float    lightBleedReduction  = 0.2f;   // Chebyshev light-bleed clamp threshold
    float    minVariance          = 1e-5f;  // minimum variance to avoid division by zero
    float    cascadeBlendRange    = 0.1f;   // fraction of cascade extent for cross-fade [0,1]
    float    perCascadeWeights[4] = {1.f, 1.f, 1.f, 1.f}; // per-cascade contribution weights
};

// ── ReSTIR GI settings ────────────────────────────────────────────────────────
struct ReSTIRSettings {
    bool     enabled         = false;
    bool     spatialReuse    = true;   // enable spatial neighbour reservoir reuse
    bool     temporalReuse   = true;   // enable temporal reservoir reprojection
    uint32_t reservoirSize   = 8;      // reservoirs maintained per pixel
    float    clampThreshold  = 10.f;   // contribution weight clamp (firefly suppression)
};

// ── ReSTIR PT settings ────────────────────────────────────────────────────────
struct ReSTIRPTSettings {
    bool     enabled                = false;
    uint32_t raysPerPixel           = 1;     // PT rays per pixel per frame
    uint32_t maxPathLength          = 6;     // maximum path vertices (bounces + 1)
    uint32_t russianRouletteDepth   = 3;     // depth at which Russian roulette begins
    bool     neeEnabled             = true;  // next-event estimation (direct light sampling)
};

// ── Camera Lens Effects (chromatic aberration + film grain) settings ──────────
struct CameraLensSettings {
    bool     chromaticAberrationEnabled = false;
    float    aberrationStrength         = 0.005f; // radial RGB channel separation (NDC units)
    bool     filmGrainEnabled           = false;
    float    grainIntensity             = 0.04f;  // additive grain amplitude
    float    grainSize                  = 1.5f;   // grain texel scale
};

// ── Screen-Space Caustics settings ───────────────────────────────────────────
struct CausticsSettings {
    bool     enabled      = false;
    uint32_t sampleCount  = 32;    // photon projection samples per pixel
    float    intensity    = 0.5f;  // additive caustic contribution scale
    float    searchRadius = 0.1f;  // world-space photon gather radius
};

// ── Hero Wavelength Spectral Dispersion settings ──────────────────────────────
struct SpectralSettings {
    bool     enabled              = false;
    uint32_t wavelengthSamples    = 4;       // hero wavelength samples per path
    float    dispersionScale      = 1.f;     // Cauchy dispersion coefficient scale
    float    heroWavelengthNm     = 550.f;   // primary hero wavelength in nanometres
};

// ── Photon Mapping settings ───────────────────────────────────────────────────
struct PhotonMappingSettings {
    bool     enabled      = false;
    uint32_t photonCount  = 100000; // photons emitted from all light sources per frame
    uint32_t maxBounces   = 4;      // maximum photon path length
    float    gatherRadius = 0.05f;  // world-space photon gather radius
};

// ── Auto-Exposure / Eye Adaptation settings ───────────────────────────────────
struct AutoExposureSettings {
    bool  enabled           = false;
    float minEV             = -4.f;  // minimum exposure value (dark scenes)
    float maxEV             =  4.f;  // maximum exposure value (bright scenes)
    float adaptationSpeed   =  1.f;  // EV change rate in stops per second
    float targetLuminance   =  0.18f; // scene-referred mid-grey target
};

// ── Mueller Matrix BSDF settings ─────────────────────────────────────────────
struct MuellerBSDFSettings {
    bool     enabled               = false;
    uint32_t matrixOrder           = 4;     // Mueller matrix dimension (always 4 for Stokes)
    bool     trackDepolarisation   = true;  // track depolarisation through scattering events
};

// ── Time-Resolved Phosphorescence Decay settings ──────────────────────────────
struct PhosphorescenceDecaySettings {
    bool     enabled        = false;
    uint32_t decayFrames    = 30;    // frame window over which emission decays to zero
    float    decayExponent  = 2.f;   // exponent of the exponential decay curve
    bool     accumulate     = true;  // accumulate emission across frames before decay
};

// ── Hyperspectral IBL settings ────────────────────────────────────────────────
struct HyperspectralIBLSettings {
    bool     enabled                = false;
    uint32_t spectralBands          = 8;    // spectral bands to sample from environment map
    uint32_t envMapMipLevels        = 6;    // mip levels in the spectral environment map
    uint32_t importanceSampleCount  = 64;   // importance samples per spectral band
};

// ── Polarisation Rendering settings ──────────────────────────────────────────
struct PolarisationSettings {
    bool     enabled          = false;
    uint32_t stokesComponents = 4;     // Stokes vector components tracked (1–4)
    bool     trackCircular    = true;  // track circular polarisation (S3 component)
};

// ── Fluorescence / Phosphorescence settings ───────────────────────────────────
struct FluorescenceSettings {
    bool     enabled                = false;
    float    fluorescenceScale      = 1.f;   // re-emission contribution scale
    float    phosphorescenceDecay   = 0.1f;  // per-frame phosphorescence decay factor
    uint32_t emissionBands          = 4;     // wavelength bands in the re-emission matrix
};

// ── Spectral Upsampling settings ──────────────────────────────────────────────
enum class SpectralUpsamplingMethod : uint8_t {
    Smits   = 0,  // Smits RGB-to-spectral upsampling (fast, physically plausible)
    Sigmoid = 1,  // Sigmoid-based smooth upsampling (higher quality, more passes)
};

struct SpectralUpsamplingSettings {
    bool                   enabled        = false;
    SpectralUpsamplingMethod method        = SpectralUpsamplingMethod::Smits;
    uint32_t               upsamplingBands = 8;  // spectral bands to produce per RGB texel
};

// ── Multi-Spectral Rendering settings ────────────────────────────────────────
struct MultiSpectralSettings {
    bool     enabled          = false;
    uint32_t wavelengthBands  = 8;       // number of spectral bands to evaluate
    float    minWavelengthNm  = 380.f;   // shortest wavelength in the rendered spectrum (nm)
    float    maxWavelengthNm  = 780.f;   // longest wavelength in the rendered spectrum (nm)
    uint32_t samplesPerBand   = 2;       // path samples per wavelength band
};

// ── Bidirectional Path Tracing (BDPT) settings ────────────────────────────────
struct BDPTSettings {
    bool     enabled          = false;
    uint32_t lightPathLength  = 4;    // maximum light sub-path vertices
    uint32_t eyePathLength    = 4;    // maximum eye sub-path vertices
    bool     mis              = true; // enable multiple importance sampling for connections
};

// ── Auto White Balance settings ───────────────────────────────────────────────
enum class AutoWhiteBalanceMethod : uint8_t {
    GrayWorld  = 0,  // assumes scene average is neutral gray
    WhitePatch = 1,  // assumes scene maximum is pure white
};

struct AutoWhiteBalanceSettings {
    bool                   enabled         = false;
    AutoWhiteBalanceMethod method          = AutoWhiteBalanceMethod::GrayWorld;
    float                  strength        = 1.f;   // blend weight for correction matrix [0,1]
    float                  adaptationSpeed = 0.5f;  // correction matrix change rate per second
};

// ── Lens Flare & Anamorphic Streak settings ───────────────────────────────────
struct LensFlareSettings {
    bool     enabled      = false;
    uint32_t ghostCount   = 4;      // number of radial ghost sprites per source
    float    streakLength = 0.8f;   // anamorphic streak length as fraction of screen width
    float    threshold    = 1.f;    // HDR luminance threshold for flare source detection
    float    intensity    = 0.15f;  // additive blend weight before tone mapping
};

// ── Atmospheric Scattering settings ──────────────────────────────────────────
struct AtmosphericScatteringSettings {
    bool     enabled         = false;
    float    rayleighScale   = 1.f;   // Rayleigh scattering coefficient scale
    float    mieScale        = 1.f;   // Mie scattering coefficient scale
    float    sunZenithAngle  = 45.f;  // sun zenith angle in degrees (0 = overhead)
    uint32_t lutSize         = 256;   // transmittance LUT side length in texels
};

// ── Light Shaft (God Ray) settings ────────────────────────────────────────────
struct LightShaftSettings {
    bool     enabled      = false;
    uint32_t sampleCount  = 64;    // radial blur samples toward sun screen-pos
    float    decay        = 0.97f; // per-sample luminance decay factor
    float    exposure     = 0.3f;  // additive composite blend weight
};

// ── GPU-Driven Clustered Lighting settings ────────────────────────────────────
struct ClusteredLightingSettings {
    bool     enabled               = false;
    uint32_t clusterDimX           = 16;   // light clusters along X
    uint32_t clusterDimY           = 8;    // light clusters along Y
    uint32_t clusterDimZ           = 24;   // depth slices in cluster grid
    uint32_t maxLightsPerCluster   = 128;  // light list capacity per cluster
};

// ── Screen-Space Global Illumination (SSGI) settings ─────────────────────────
struct SSGISettings {
    bool     enabled      = false;
    uint32_t rayCount     = 4;      // hemisphere rays per pixel
    float    rayLength    = 2.f;    // world-space ray march length
    float    maxRoughness = 0.8f;   // skip SSGI on surfaces rougher than this
};

// ── Decal Rendering settings ──────────────────────────────────────────────────
struct DecalSettings {
    bool     enabled         = false;
    uint32_t maxDecals       = 256;   // maximum projected decals per frame
    uint32_t atlasResolution = 2048;  // decal atlas texture side length in pixels
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
    bool        enableSSS                = false; // screen-space subsurface scattering blur
    bool        enableContactShadows     = false; // short-range depth-buffer contact shadows
    bool        enableTiledLighting      = false; // 16×16 tile light classification pass
    bool        enableClusteredLighting  = false; // 3-D view-space cluster classification
    bool        enableSSGI              = false; // screen-space global illumination gather
    bool        enableDecals            = false; // projected decal resolve into GBuffer
    bool        enableRTGI                  = false; // hardware ray-traced global illumination
    bool        enableAtmosphericScattering = false; // Rayleigh+Mie transmittance LUT pass
    bool        enableLightShafts           = false; // radial blur god-ray composite pass
    bool        enableHBAO                  = false; // horizon-based ambient occlusion (HBAO+)
    bool        enableVSM                   = false; // variance shadow maps soft-shadow filter
    bool        enableReSTIR                = false; // spatiotemporal reservoir resampling (ReSTIR GI)
    bool        enableLensFlare             = false; // lens flare + anamorphic streak pass
    bool        enableReSTIRPT             = false; // ReSTIR path tracing (specular/caustic paths)
    bool        enableCameraLens           = false; // chromatic aberration + film grain
    bool        enableCaustics             = false; // screen-space caustic projection pass
    bool        enableSpectral             = false; // hero wavelength spectral dispersion
    bool        enablePhotonMapping        = false; // progressive photon map global caustics
    bool        enableAutoExposure         = false; // GPU luminance histogram + EV adaptation
    bool        enableMultiSpectral        = false; // full N-wavelength multi-spectral rendering
    bool        enableBDPT                 = false; // bidirectional path tracing connections
    bool        enableAutoWhiteBalance     = false; // histogram-based auto white balance
    bool        enablePolarisation         = false; // Stokes vector polarisation tracking
    bool        enableFluorescence         = false; // fluorescence / phosphorescence re-emission
    bool        enableSpectralUpsampling   = false; // RGB-to-spectral texture upsampling
    bool        enableMuellerBSDF          = false; // Mueller matrix 4×4 BSDF evaluation
    bool        enablePhosphorescenceDecay = false; // time-resolved phosphorescence decay accumulation
    bool        enableHyperspectralIBL     = false; // spectral-band environment map IBL
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
    bool     sssActive            = false;                            // true when SSS blur dispatch ran
    uint32_t sssBlurPasses        = 0;                                // separable blur passes fired this frame
    bool     contactShadowsActive  = false;                           // true when contact shadow march ran
    uint32_t contactShadowRayCount = 0;                               // rays cast this frame (width × height)
    bool     tiledLightingActive   = false;                           // true when tile classification ran
    uint32_t lightTileCount        = 0;                               // total tiles classified this frame
    uint32_t maxLightsPerTile      = 0;                               // peak lights in any tile this frame
    bool     clusteredLightingActive       = false;                   // true when cluster classification ran
    uint32_t lightClusterCount             = 0;                       // total X×Y×Z clusters classified
    uint32_t clusteredMaxLightsPerCluster  = 0;                       // peak lights in any cluster this frame
    bool     ssgiActive            = false;                           // true when SSGI gather dispatch ran
    uint32_t ssgiRayCount          = 0;                               // width × height × rayCount rays cast
    bool     decalsActive          = false;                           // true when decal projection pass ran
    uint32_t decalCount            = 0;                               // decals projected this frame
    bool     rtgiActive            = false;                           // true when RTGI dispatch ran
    uint32_t rtgiRaysDispatched    = 0;                               // width × height × raysPerPixel
    bool     atmosphericScatteringActive = false;                     // true when transmittance LUT dispatch ran
    uint32_t atmosphericLUTSize    = 0;                               // transmittance LUT side length this frame
    bool     lightShaftsActive     = false;                           // true when radial blur pass ran
    uint32_t lightShaftSampleCount = 0;                               // radial samples dispatched this frame
    uint32_t rtgiBounceCount       = 0;                               // RTGI bounces dispatched this frame
    bool     hbaoActive            = false;                           // true when HBAO+ dispatch ran
    uint32_t hbaoSampleCount       = 0;                               // width × height × sliceCount × stepCount
    bool     vsmActive                  = false;                      // true when VSM blur+resolve ran
    uint32_t vsmCascadeCount            = 0;                          // shadow cascades processed through VSM
    uint32_t vsmBlendedCascadeCount     = 0;                          // cascades with active cross-fade blending
    bool     restirActive               = false;                      // true when ReSTIR pass ran
    uint32_t restirReservoirCount       = 0;                          // width × height × reservoirSize
    bool     lensFlareActive            = false;                      // true when lens flare pass ran
    uint32_t lensFlareGhostCount        = 0;                          // ghost sprites composited this frame
    bool     restirPTActive            = false;                       // true when ReSTIR PT dispatch ran
    uint32_t restirPTPathCount         = 0;                           // width × height × raysPerPixel
    bool     cameraLensActive          = false;                       // true when aberration or grain ran
    uint32_t cameraLensPassCount       = 0;                           // sub-passes dispatched (1 or 2)
    bool     causticsActive            = false;                       // true when caustic projection ran
    uint32_t causticsSampleCount       = 0;                           // photon samples accumulated
    bool     spectralActive            = false;                       // true when spectral dispatch ran
    uint32_t spectralWavelengthSamples = 0;                           // wavelength samples evaluated this frame
    bool     photonMappingActive       = false;                       // true when photon trace + gather ran
    uint32_t photonsEmitted            = 0;                           // photons traced from light sources
    bool     autoExposureActive        = false;                       // true when histogram + EV adaptation ran
    float    autoExposureEV            = 0.f;                         // computed EV applied this frame
    bool     multiSpectralActive       = false;                       // true when multi-spectral dispatch ran
    uint32_t multiSpectralBandCount    = 0;                           // wavelength bands evaluated this frame
    bool     bdptActive                = false;                       // true when BDPT passes ran
    uint32_t bdptConnectionCount       = 0;                           // light–eye path connections this frame
    bool     autoWhiteBalanceActive    = false;                       // true when AWB histogram + correction ran
    AutoWhiteBalanceMethod autoWhiteBalanceMethod = AutoWhiteBalanceMethod::GrayWorld; // method used this frame
    bool     polarisationActive        = false;                       // true when Stokes vector dispatch ran
    uint32_t polarisationRayCount      = 0;                           // polarised rays evaluated this frame
    bool     fluorescenceActive        = false;                       // true when fluorescence re-emission ran
    uint32_t fluorescenceEmissionBands = 0;                           // emission bands evaluated this frame
    bool     spectralUpsamplingActive  = false;                       // true when RGB→spectral upsampling ran
    SpectralUpsamplingMethod spectralUpsamplingMethod = SpectralUpsamplingMethod::Smits; // method used this frame
    bool     muellerBSDFActive         = false;                       // true when Mueller matrix dispatch ran
    uint32_t muellerBSDFEvalCount      = 0;                           // matrix evaluations this frame
    bool     phosphorescenceDecayActive = false;                      // true when decay accumulation ran
    uint32_t phosphorescenceDecayFrames = 0;                          // configured decay frame window
    bool     hyperspectralIBLActive    = false;                       // true when spectral env dispatch ran
    uint32_t hyperspectralIBLBandCount = 0;                           // spectral bands sampled from env map
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

    void setSSSSettings(const SSSSettings& settings) noexcept;
    [[nodiscard]] const SSSSettings& sssSettings() const noexcept;

    void setContactShadowSettings(const ContactShadowSettings& settings) noexcept;
    [[nodiscard]] const ContactShadowSettings& contactShadowSettings() const noexcept;

    void setTiledLightingSettings(const TiledLightingSettings& settings) noexcept;
    [[nodiscard]] const TiledLightingSettings& tiledLightingSettings() const noexcept;

    void setClusteredLightingSettings(const ClusteredLightingSettings& settings) noexcept;
    [[nodiscard]] const ClusteredLightingSettings& clusteredLightingSettings() const noexcept;

    void setSSGISettings(const SSGISettings& settings) noexcept;
    [[nodiscard]] const SSGISettings& ssgiSettings() const noexcept;

    void setDecalSettings(const DecalSettings& settings) noexcept;
    [[nodiscard]] const DecalSettings& decalSettings() const noexcept;

    void setRTGISettings(const RTGISettings& settings) noexcept;
    [[nodiscard]] const RTGISettings& rtgiSettings() const noexcept;

    void setAtmosphericScatteringSettings(const AtmosphericScatteringSettings& settings) noexcept;
    [[nodiscard]] const AtmosphericScatteringSettings& atmosphericScatteringSettings() const noexcept;

    void setLightShaftSettings(const LightShaftSettings& settings) noexcept;
    [[nodiscard]] const LightShaftSettings& lightShaftSettings() const noexcept;

    void setHBAOSettings(const HBAOSettings& settings) noexcept;
    [[nodiscard]] const HBAOSettings& hbaoSettings() const noexcept;

    void setVSMSettings(const VSMSettings& settings) noexcept;
    [[nodiscard]] const VSMSettings& vsmSettings() const noexcept;

    void setReSTIRSettings(const ReSTIRSettings& settings) noexcept;
    [[nodiscard]] const ReSTIRSettings& reSTIRSettings() const noexcept;

    void setLensFlareSettings(const LensFlareSettings& settings) noexcept;
    [[nodiscard]] const LensFlareSettings& lensFlareSettings() const noexcept;

    void setReSTIRPTSettings(const ReSTIRPTSettings& settings) noexcept;
    [[nodiscard]] const ReSTIRPTSettings& reSTIRPTSettings() const noexcept;

    void setCameraLensSettings(const CameraLensSettings& settings) noexcept;
    [[nodiscard]] const CameraLensSettings& cameraLensSettings() const noexcept;

    void setCausticsSettings(const CausticsSettings& settings) noexcept;
    [[nodiscard]] const CausticsSettings& causticsSettings() const noexcept;

    void setSpectralSettings(const SpectralSettings& settings) noexcept;
    [[nodiscard]] const SpectralSettings& spectralSettings() const noexcept;

    void setPhotonMappingSettings(const PhotonMappingSettings& settings) noexcept;
    [[nodiscard]] const PhotonMappingSettings& photonMappingSettings() const noexcept;

    void setAutoExposureSettings(const AutoExposureSettings& settings) noexcept;
    [[nodiscard]] const AutoExposureSettings& autoExposureSettings() const noexcept;

    void setMuellerBSDFSettings(const MuellerBSDFSettings& settings) noexcept;
    [[nodiscard]] const MuellerBSDFSettings& muellerBSDFSettings() const noexcept;

    void setPhosphorescenceDecaySettings(const PhosphorescenceDecaySettings& settings) noexcept;
    [[nodiscard]] const PhosphorescenceDecaySettings& phosphorescenceDecaySettings() const noexcept;

    void setHyperspectralIBLSettings(const HyperspectralIBLSettings& settings) noexcept;
    [[nodiscard]] const HyperspectralIBLSettings& hyperspectralIBLSettings() const noexcept;

    void setPolarisationSettings(const PolarisationSettings& settings) noexcept;
    [[nodiscard]] const PolarisationSettings& polarisationSettings() const noexcept;

    void setFluorescenceSettings(const FluorescenceSettings& settings) noexcept;
    [[nodiscard]] const FluorescenceSettings& fluorescenceSettings() const noexcept;

    void setSpectralUpsamplingSettings(const SpectralUpsamplingSettings& settings) noexcept;
    [[nodiscard]] const SpectralUpsamplingSettings& spectralUpsamplingSettings() const noexcept;

    void setMultiSpectralSettings(const MultiSpectralSettings& settings) noexcept;
    [[nodiscard]] const MultiSpectralSettings& multiSpectralSettings() const noexcept;

    void setBDPTSettings(const BDPTSettings& settings) noexcept;
    [[nodiscard]] const BDPTSettings& bdptSettings() const noexcept;

    void setAutoWhiteBalanceSettings(const AutoWhiteBalanceSettings& settings) noexcept;
    [[nodiscard]] const AutoWhiteBalanceSettings& autoWhiteBalanceSettings() const noexcept;

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
