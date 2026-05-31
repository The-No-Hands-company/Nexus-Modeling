// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — DescriptorBinder implementation (Month 14 Track 1)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/DescriptorBinder.h>

#include <array>

namespace nexus::render {

// ── CompositeDescriptorSet ────────────────────────────────────────────────────

bool CompositeDescriptorSet::update(nexus::gfx::IDevice& dev,
                                    const CompositeDescriptorInputs& inputs)
{
    if (!inputs.albedo.valid()   || !inputs.normal.valid()  ||
        !inputs.velocity.valid() || !inputs.depth.valid()   ||
        !inputs.sampler.valid()) {
        return false;
    }

    const std::array<nexus::gfx::DescriptorBindingDesc, 5> bindings = {{
        { 0, nexus::gfx::DescriptorType::SampledTexture, {}, inputs.albedo,   {} },
        { 1, nexus::gfx::DescriptorType::SampledTexture, {}, inputs.normal,   {} },
        { 2, nexus::gfx::DescriptorType::SampledTexture, {}, inputs.velocity, {} },
        { 3, nexus::gfx::DescriptorType::SampledTexture, {}, inputs.depth,    {} },
        { 4, nexus::gfx::DescriptorType::Sampler,        {}, {},              inputs.sampler },
    }};

    if (!m_set.valid()) {
        nexus::gfx::DescriptorSetDesc desc{};
        desc.bindings   = std::span<const nexus::gfx::DescriptorBindingDesc>(bindings);
        desc.debugName  = "CompositeDescriptorSet";
        m_set = dev.allocateDescriptorSet(desc);
        if (!m_set.valid()) return false;
    }

    dev.updateDescriptorSet(m_set, std::span<const nexus::gfx::DescriptorBindingDesc>(bindings));
    return true;
}

void CompositeDescriptorSet::destroy(nexus::gfx::IDevice& dev) noexcept
{
    if (m_set.valid()) {
        dev.freeDescriptorSet(m_set);
        m_set = {};
    }
}

// ── MaterialTableDescriptorSet ────────────────────────────────────────────────

bool MaterialTableDescriptorSet::update(nexus::gfx::IDevice& dev,
                                        nexus::gfx::BufferHandle buffer,
                                        uint32_t offsetBytes,
                                        uint32_t capacityBytes)
{
    if (!buffer.valid()) return false;
    if (offsetBytes % 4 != 0) return false;
    if (capacityBytes > 0 && offsetBytes >= capacityBytes) return false;

    const nexus::gfx::DescriptorBindingDesc binding{
        0, nexus::gfx::DescriptorType::UniformBuffer, buffer, {}, {}
    };

    if (!m_set.valid()) {
        nexus::gfx::DescriptorSetDesc desc{};
        desc.bindings  = std::span<const nexus::gfx::DescriptorBindingDesc>(&binding, 1);
        desc.debugName = "MaterialTableDescriptorSet";
        m_set = dev.allocateDescriptorSet(desc);
        if (!m_set.valid()) return false;
    }

    dev.updateDescriptorSet(m_set,
        std::span<const nexus::gfx::DescriptorBindingDesc>(&binding, 1));
    m_offsetBytes = offsetBytes;
    return true;
}

void MaterialTableDescriptorSet::destroy(nexus::gfx::IDevice& dev) noexcept
{
    if (m_set.valid()) {
        dev.freeDescriptorSet(m_set);
        m_set = {};
    }
    m_offsetBytes = 0;
}

// ── ShadowDescriptorSet ───────────────────────────────────────────────────────

bool ShadowDescriptorSet::update(nexus::gfx::IDevice& dev,
                                 nexus::gfx::TextureHandle depthTexture,
                                 nexus::gfx::SamplerHandle depthSampler,
                                 nexus::gfx::BufferHandle  lightingBuffer,
                                 uint32_t cascadeCount)
{
    if (!depthTexture.valid() || !depthSampler.valid() ||
        !lightingBuffer.valid() || cascadeCount == 0) {
        return false;
    }

    const std::array<nexus::gfx::DescriptorBindingDesc, 3> bindings = {{
        { 0, nexus::gfx::DescriptorType::SampledTexture, {}, depthTexture,    {} },
        { 1, nexus::gfx::DescriptorType::Sampler,        {}, {},              depthSampler },
        { 2, nexus::gfx::DescriptorType::UniformBuffer,  lightingBuffer, {}, {} },
    }};

    if (!m_set.valid()) {
        nexus::gfx::DescriptorSetDesc desc{};
        desc.bindings  = std::span<const nexus::gfx::DescriptorBindingDesc>(bindings);
        desc.debugName = "ShadowDescriptorSet";
        m_set = dev.allocateDescriptorSet(desc);
        if (!m_set.valid()) return false;
    }

    dev.updateDescriptorSet(m_set,
        std::span<const nexus::gfx::DescriptorBindingDesc>(bindings));
    m_cascadeCount = cascadeCount;
    return true;
}

void ShadowDescriptorSet::destroy(nexus::gfx::IDevice& dev) noexcept
{
    if (m_set.valid()) {
        dev.freeDescriptorSet(m_set);
        m_set = {};
    }
    m_cascadeCount = 0;
}

} // namespace nexus::render
