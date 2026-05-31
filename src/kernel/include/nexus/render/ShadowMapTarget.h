#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Shadow Map Target Lifecycle (Month 14 Track 2)
//
//  ShadowMapTarget manages the shadow depth texture and depth sampler via
//  IDevice.  It owns the resource handles and provides a well-defined
//  create/resize/destroy lifecycle.
//
//  Design rules:
//  - create() allocates the depth texture and sampler; returns false if the
//    device rejects the allocation (handle invalid after call).
//  - resize() destroys existing handles before allocating new ones so there
//    is never a handle leak across resize.
//  - destroy() is safe to call multiple times; subsequent calls are no-ops.
//  - Callers must call destroy(dev) before the IDevice is torn down.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/Types.h>
#include <cstdint>

namespace nexus::render {

struct ShadowMapTargetDesc {
    nexus::gfx::Extent2D extent       = {1024u, 1024u};
    uint32_t             cascadeCount = 1;
    // Depth format: maps to D32_SFLOAT in Vulkan, depth32 on other backends.
    // Exposed as a uint32 tag so the descriptor is backend-agnostic.
    uint32_t             depthFormatTag = 0; // 0 = default (D32_SFLOAT)
};

class ShadowMapTarget {
public:
    ShadowMapTarget() = default;
    ~ShadowMapTarget() = default;

    // Non-copyable; movable.
    ShadowMapTarget(const ShadowMapTarget&)            = delete;
    ShadowMapTarget& operator=(const ShadowMapTarget&) = delete;
    ShadowMapTarget(ShadowMapTarget&&)                 = default;
    ShadowMapTarget& operator=(ShadowMapTarget&&)      = default;

    // Allocates the depth texture and sampler.  Returns false if extent is
    // zero in either dimension or cascadeCount is 0, or if the device
    // returns an invalid handle.
    [[nodiscard]] bool create(nexus::gfx::IDevice& dev, const ShadowMapTargetDesc& desc);

    // Destroys existing handles then re-allocates with the new extent.
    // cascadeCount from the original desc is preserved.
    // Returns false (and leaves target destroyed) if new extent is invalid.
    [[nodiscard]] bool resize(nexus::gfx::IDevice& dev, nexus::gfx::Extent2D newExtent);

    // Frees both handles.  Safe to call on an already-destroyed target.
    void destroy(nexus::gfx::IDevice& dev) noexcept;

    [[nodiscard]] bool isReady() const noexcept {
        return m_depthTexture.valid() && m_depthSampler.valid();
    }

    [[nodiscard]] nexus::gfx::TextureHandle depthTexture() const noexcept { return m_depthTexture; }
    [[nodiscard]] nexus::gfx::SamplerHandle depthSampler() const noexcept { return m_depthSampler; }
    [[nodiscard]] nexus::gfx::Extent2D      extent()       const noexcept { return m_extent; }
    [[nodiscard]] uint32_t                  cascadeCount() const noexcept { return m_cascadeCount; }

private:
    nexus::gfx::TextureHandle m_depthTexture{};
    nexus::gfx::SamplerHandle m_depthSampler{};
    nexus::gfx::Extent2D      m_extent{};
    uint32_t                  m_cascadeCount = 0;
};

} // namespace nexus::render
