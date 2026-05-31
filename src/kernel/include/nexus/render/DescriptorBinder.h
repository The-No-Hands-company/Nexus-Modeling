#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Descriptor and Material Resource Binding (Month 14 Track 1)
//
//  Provides three RAII descriptor-set wrappers that own the lifecycle of the
//  descriptor set allocated from IDevice.  Each wrapper:
//
//    - Allocates its DescriptorSetHandle on the first update() call.
//    - Updates bindings whenever update() is called with new resource handles.
//    - Frees the handle in destroy() or on destruction (if not yet freed).
//    - Exposes isReady() to let callers validate before recording draw calls.
//
//  All three types work on any IDevice backend (Null, Vulkan, future).
//  No ICommandBuffer or pipeline handle dependencies; this layer is
//  allocation-and-update only.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/Types.h>
#include <cstdint>

namespace nexus::render {

// ── CompositeDescriptorSet ────────────────────────────────────────────────────
//
// Manages the descriptor set that binds the four GBuffer textures (albedo,
// normal, velocity, depth) plus a shared sampler to the composite pass.
//
// Binding layout (fixed):
//   0 — SampledTexture  albedo
//   1 — SampledTexture  normal
//   2 — SampledTexture  velocity
//   3 — SampledTexture  depth
//   4 — Sampler         shared sampler
//
struct CompositeDescriptorInputs {
    nexus::gfx::TextureHandle albedo;
    nexus::gfx::TextureHandle normal;
    nexus::gfx::TextureHandle velocity;
    nexus::gfx::TextureHandle depth;
    nexus::gfx::SamplerHandle sampler;
};

class CompositeDescriptorSet {
public:
    CompositeDescriptorSet() = default;
    ~CompositeDescriptorSet() = default;

    // Non-copyable; movable.
    CompositeDescriptorSet(const CompositeDescriptorSet&)            = delete;
    CompositeDescriptorSet& operator=(const CompositeDescriptorSet&) = delete;
    CompositeDescriptorSet(CompositeDescriptorSet&&)                 = default;
    CompositeDescriptorSet& operator=(CompositeDescriptorSet&&)      = default;

    // Allocates (if needed) and updates the descriptor set from dev.
    // Returns false if any required handle is invalid.
    [[nodiscard]] bool update(nexus::gfx::IDevice& dev,
                              const CompositeDescriptorInputs& inputs);

    // Frees the descriptor set back to dev.  Safe to call multiple times.
    void destroy(nexus::gfx::IDevice& dev) noexcept;

    [[nodiscard]] bool isReady() const noexcept { return m_set.valid(); }
    [[nodiscard]] nexus::gfx::DescriptorSetHandle descriptorSet() const noexcept { return m_set; }

private:
    nexus::gfx::DescriptorSetHandle m_set{};
};

// ── MaterialTableDescriptorSet ────────────────────────────────────────────────
//
// Manages the descriptor set that binds the material table buffer.
//
// Binding layout (fixed):
//   0 — UniformBuffer   materialTable  (offset tracked separately)
//
// update() fails (returns false) if buffer is invalid or offsetBytes exceeds
// the stated capacity (capacity = 0 means unbounded, offset must still be
// aligned to 4 bytes).
//
class MaterialTableDescriptorSet {
public:
    MaterialTableDescriptorSet() = default;
    ~MaterialTableDescriptorSet() = default;

    MaterialTableDescriptorSet(const MaterialTableDescriptorSet&)            = delete;
    MaterialTableDescriptorSet& operator=(const MaterialTableDescriptorSet&) = delete;
    MaterialTableDescriptorSet(MaterialTableDescriptorSet&&)                 = default;
    MaterialTableDescriptorSet& operator=(MaterialTableDescriptorSet&&)      = default;

    // buffer must be valid.  offsetBytes must be 4-byte aligned.
    // capacityBytes == 0 skips the range check.
    [[nodiscard]] bool update(nexus::gfx::IDevice& dev,
                              nexus::gfx::BufferHandle buffer,
                              uint32_t offsetBytes,
                              uint32_t capacityBytes = 0);

    void destroy(nexus::gfx::IDevice& dev) noexcept;

    [[nodiscard]] bool isReady() const noexcept { return m_set.valid(); }
    [[nodiscard]] nexus::gfx::DescriptorSetHandle descriptorSet() const noexcept { return m_set; }
    [[nodiscard]] uint32_t offsetBytes() const noexcept { return m_offsetBytes; }

private:
    nexus::gfx::DescriptorSetHandle m_set{};
    uint32_t m_offsetBytes = 0;
};

// ── ShadowDescriptorSet ───────────────────────────────────────────────────────
//
// Manages the descriptor set that binds the shadow depth texture, sampler, and
// per-cascade lighting buffer to the shadow-receiving composite input.
//
// Binding layout (fixed):
//   0 — SampledTexture  shadowDepthTexture
//   1 — Sampler         shadowDepthSampler
//   2 — UniformBuffer   shadowLightingBuffer
//
// update() fails if any handle is invalid OR cascadeCount == 0.
//
class ShadowDescriptorSet {
public:
    ShadowDescriptorSet() = default;
    ~ShadowDescriptorSet() = default;

    ShadowDescriptorSet(const ShadowDescriptorSet&)            = delete;
    ShadowDescriptorSet& operator=(const ShadowDescriptorSet&) = delete;
    ShadowDescriptorSet(ShadowDescriptorSet&&)                 = default;
    ShadowDescriptorSet& operator=(ShadowDescriptorSet&&)      = default;

    [[nodiscard]] bool update(nexus::gfx::IDevice& dev,
                              nexus::gfx::TextureHandle depthTexture,
                              nexus::gfx::SamplerHandle depthSampler,
                              nexus::gfx::BufferHandle  lightingBuffer,
                              uint32_t cascadeCount);

    void destroy(nexus::gfx::IDevice& dev) noexcept;

    [[nodiscard]] bool isReady() const noexcept { return m_set.valid(); }
    [[nodiscard]] nexus::gfx::DescriptorSetHandle descriptorSet() const noexcept { return m_set; }
    [[nodiscard]] uint32_t cascadeCount() const noexcept { return m_cascadeCount; }

private:
    nexus::gfx::DescriptorSetHandle m_set{};
    uint32_t m_cascadeCount = 0;
};

} // namespace nexus::render
