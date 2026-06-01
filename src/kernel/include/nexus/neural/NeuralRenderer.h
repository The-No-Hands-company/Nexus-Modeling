#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Neural — Neural Rendering Infrastructure
//
//  Provides hardware-accelerated or software-fallback implementations of:
//    • Inline denoising (DLSS 4 / XeSS / OIDN)
//    • Temporal upscaling
//    • Neural texture compression (NTC) decode hint
//
//  On Low/Mid tier hardware, all paths gracefully fall back to
//  CPU-side Intel OIDN or bilinear upscaling — zero hard requirements.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <memory>

// Forward declaration — avoids pulling in the full Device.h
namespace nexus::gfx { class IDevice; }

namespace nexus::neural {

// ── Denoiser backend ──────────────────────────────────────────────────────────
enum class DenoiserBackend : uint8_t {
    None,
    OIDN_CPU,    // Intel Open Image Denoise — always available fallback
    DLSS4,       // NVIDIA DLSS 4 temporal denoiser
    DLSS_RR,     // NVIDIA DLSS Ray Reconstruction (NVSDK_NGX_Feature_RayReconstruction)
    XeSS,        // Intel XeSS Super Sampling
    FSR3,        // AMD FidelityFX Super Resolution 3
};

// ── Upscaler backend ──────────────────────────────────────────────────────────
enum class UpscalerBackend : uint8_t {
    None,
    Bilinear,    // software fallback — always available
    DLSS4,
    XeSS,
    FSR3,
};

// ── Denoiser input ────────────────────────────────────────────────────────────
struct DenoiserInput {
    nexus::gfx::TextureHandle color;         // noisy HDR color
    nexus::gfx::TextureHandle albedo;        // optional — improves quality
    nexus::gfx::TextureHandle normal;        // optional — world-space normals
    nexus::gfx::TextureHandle motionVectors; // for temporal denoising
    float  exposureScale = 1.f;
};

struct DenoiserOutput {
    nexus::gfx::TextureHandle color;         // denoised HDR result
};

// ── Upscaler input ────────────────────────────────────────────────────────────
struct UpscalerInput {
    nexus::gfx::TextureHandle color;
    nexus::gfx::TextureHandle depth;
    nexus::gfx::TextureHandle motionVectors;
    nexus::gfx::Extent2D      renderResolution;
    nexus::gfx::Extent2D      outputResolution;
    float jitterX = 0.f, jitterY = 0.f;
    bool  reset   = false;  // set on camera cut / scene load
};

struct UpscalerOutput {
    nexus::gfx::TextureHandle color;
};

// ── INeuralRenderer ───────────────────────────────────────────────────────────
class INeuralRenderer {
public:
    virtual ~INeuralRenderer() = default;

    [[nodiscard]] virtual DenoiserBackend activeDenoiser() const noexcept = 0;
    [[nodiscard]] virtual UpscalerBackend activeUpscaler() const noexcept = 0;

    virtual void denoise (nexus::gfx::CmdBufHandle cmd, const DenoiserInput&  in, DenoiserOutput&  out) = 0;
    virtual void upscale (nexus::gfx::CmdBufHandle cmd, const UpscalerInput&  in, UpscalerOutput&  out) = 0;
};

// ── NeuralBackend — explicit backend selection ────────────────────────────────
enum class NeuralBackend : uint8_t {
    Auto,       // auto-select: DLSS4 → XeSS → OIDN_CPU → Bilinear
    DLSS4,      // require DLSS 4; falls back to Bilinear if unavailable
    DLSS_RR,    // DLSS Ray Reconstruction; falls back to Bilinear if unavailable
    XeSS,       // require XeSS; falls back to Bilinear if unavailable
    OIDN_CPU,   // Intel Open Image Denoise (CPU)
    FSR3,       // AMD FidelityFX Super Resolution 3; falls back to Bilinear if unavailable
    Bilinear,   // deterministic software fallback — always available
};

// ── NeuralRendererFactory ─────────────────────────────────────────────────────
struct NeuralRendererFactory {
    // Create with an explicit backend preference. Never returns nullptr.
    [[nodiscard]] static std::unique_ptr<INeuralRenderer> create(
        NeuralBackend backend, nexus::gfx::IDevice& device);
};

// ── Legacy factory — auto-selects best available backend ──────────────────────
std::unique_ptr<INeuralRenderer> createNeuralRenderer(
    nexus::gfx::IDevice& device,
    bool preferDLSS = true,
    bool preferXeSS = true,
    bool preferFSR  = true
);

} // namespace nexus::neural
