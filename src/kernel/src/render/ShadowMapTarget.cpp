// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — ShadowMapTarget implementation (Month 14 Track 2)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/ShadowMapTarget.h>

namespace nexus::render {

bool ShadowMapTarget::create(nexus::gfx::IDevice& dev, const ShadowMapTargetDesc& desc)
{
    if (desc.extent.width == 0 || desc.extent.height == 0 || desc.cascadeCount == 0)
        return false;

    nexus::gfx::TextureDesc texDesc{};
    texDesc.extent       = { desc.extent.width, desc.extent.height, 1u };
    texDesc.mipLevels    = 1;
    texDesc.arrayLayers  = desc.cascadeCount;
    texDesc.format       = nexus::gfx::Format::D32_Float;
    texDesc.usage        = nexus::gfx::TextureUsage::DepthAttachment
                         | nexus::gfx::TextureUsage::Sampled;

    nexus::gfx::SamplerDesc sampDesc{};
    sampDesc.magFilter  = nexus::gfx::SamplerFilter::Linear;
    sampDesc.minFilter  = nexus::gfx::SamplerFilter::Linear;
    sampDesc.addressU   = nexus::gfx::SamplerAddressMode::ClampToEdge;
    sampDesc.addressV   = nexus::gfx::SamplerAddressMode::ClampToEdge;
    sampDesc.addressW   = nexus::gfx::SamplerAddressMode::ClampToEdge;

    m_depthTexture = dev.createTexture(texDesc);
    if (!m_depthTexture.valid()) return false;

    m_depthSampler = dev.createSampler(sampDesc);
    if (!m_depthSampler.valid()) {
        dev.destroyTexture(m_depthTexture);
        m_depthTexture = {};
        return false;
    }

    m_extent       = desc.extent;
    m_cascadeCount = desc.cascadeCount;
    return true;
}

bool ShadowMapTarget::resize(nexus::gfx::IDevice& dev, nexus::gfx::Extent2D newExtent)
{
    if (newExtent.width == 0 || newExtent.height == 0) {
        destroy(dev);
        return false;
    }

    const uint32_t cascades = m_cascadeCount > 0 ? m_cascadeCount : 1u;
    destroy(dev);

    ShadowMapTargetDesc desc{};
    desc.extent       = newExtent;
    desc.cascadeCount = cascades;
    return create(dev, desc);
}

void ShadowMapTarget::destroy(nexus::gfx::IDevice& dev) noexcept
{
    if (m_depthTexture.valid()) {
        dev.destroyTexture(m_depthTexture);
        m_depthTexture = {};
    }
    if (m_depthSampler.valid()) {
        dev.destroySampler(m_depthSampler);
        m_depthSampler = {};
    }
    m_extent       = {};
    m_cascadeCount = 0;
}

} // namespace nexus::render
