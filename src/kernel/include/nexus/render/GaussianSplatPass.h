#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Gaussian Splat Pass (Month 13 first slice)
//
//  Backend-agnostic CPU staging step that promotes nexus::gfx::GaussianSplat-
//  SceneNode payloads into the Renderer per-frame pipeline.  This first slice
//  produces deterministic counters (submitted / projected / discarded splats,
//  per-cloud splat draw calls) that flow into FrameStats and frame capture so
//  downstream tooling can validate the splat path independently of any GPU
//  recording.  No IDevice or ICommandBuffer contracts change.
//
//  Reversed-Z and pass ordering invariants are preserved: the splat pass runs
//  after the composite pass and never alters GBuffer or swapchain layouts.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/GaussianSplatting.h>
#include <nexus/render/Camera.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nexus::render {

// ── Pass configuration ────────────────────────────────────────────────────────
struct GaussianSplatPassConfig {
    Mat4  view       = Mat4::identity();
    Mat4  projection = Mat4::identity();
    float splatScaleMultiplier = 1.f;
    nexus::gfx::SplatSortMode sortMode = nexus::gfx::SplatSortMode::ViewDepthCPU;
    // Logit-space opacity cutoff.  Splats with stored opacity below this value
    // are treated as fully transparent and discarded by the pass.  The default
    // (-5.5413) maps through sigmoid to ≈ 1/255, matching 8-bit alpha epsilon.
    float opacityCutoffLogit = -5.5413f;
};

// ── Per-frame stats ───────────────────────────────────────────────────────────
struct GaussianSplatPassStats {
    std::uint32_t splatDrawCalls  = 0; // one per attached, visible, non-empty cloud that has any projected splat
    std::uint32_t submittedSplats = 0; // total splats across all attached visible clouds
    std::uint32_t projectedSplats = 0; // splats inside the clip volume and above opacity cutoff
    std::uint32_t discardedSplats = 0; // submittedSplats - projectedSplats
};

// ── Pass object ───────────────────────────────────────────────────────────────
class GaussianSplatPass {
public:
    GaussianSplatPass() = default;

    // ── Cloud attachment ──────────────────────────────────────────────────────
    // The pass does not own the scene nodes; callers must keep them alive for
    // the lifetime of the pass or until clearClouds() is invoked.  Passing
    // nullptr is a no-op (defensive).
    void                       addCloud(const nexus::gfx::GaussianSplatSceneNode* node);
    void                       clearClouds() noexcept;
    [[nodiscard]] std::size_t  attachedCloudCount() const noexcept;

    // ── Configuration ─────────────────────────────────────────────────────────
    void setConfig(const GaussianSplatPassConfig& cfg) noexcept;
    // Convenience hook for the Renderer to refresh camera matrices per-frame
    // without disturbing user-set scale / sort / opacity settings.
    void setCameraMatrices(const Mat4& view, const Mat4& projection) noexcept;
    [[nodiscard]] const GaussianSplatPassConfig& config() const noexcept;

    // ── Deterministic stats ───────────────────────────────────────────────────
    // Walks the attached clouds under the current config and returns the
    // per-frame counters.  Pure CPU work; result is bit-stable across runs for
    // identical inputs.
    [[nodiscard]] GaussianSplatPassStats computeStats() const;

private:
    std::vector<const nexus::gfx::GaussianSplatSceneNode*> m_clouds;
    GaussianSplatPassConfig                                m_config{};
};

} // namespace nexus::render
