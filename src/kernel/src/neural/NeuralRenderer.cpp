// ─────────────────────────────────────────────────────────────────────────────
//  NeuralRenderer — factory implementation
//  Priority: DLSS4 → XeSS → OIDN → deterministic software fallback
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/neural/NeuralRenderer.h>
#include <nexus/gfx/Device.h>
#include "OIDNDenoiser.h"
#include "FSR3Plugin.h"
#ifdef NEXUS_BACKEND_VULKAN
#  include "DLSSPlugin.h"
#  include "XeSSPlugin.h"
#endif
#include <memory>

#if defined(NEXUS_BACKEND_VULKAN)
#include "../backend/vulkan/VulkanDevice.h"
#endif

namespace nexus::neural {

namespace {

class DeterministicFallbackNeuralRenderer final : public INeuralRenderer {
public:
    [[nodiscard]] DenoiserBackend activeDenoiser() const noexcept override
    {
        return DenoiserBackend::None;
    }

    [[nodiscard]] UpscalerBackend activeUpscaler() const noexcept override
    {
        return UpscalerBackend::Bilinear;
    }

    void denoise(nexus::gfx::CmdBufHandle,
                 const DenoiserInput& in,
                 DenoiserOutput& out) override
    {
        // Deterministic fallback path: preserve the source color when no executable
        // neural denoiser is available in the current runtime.
        out.color = in.color;
    }

    void upscale(nexus::gfx::CmdBufHandle,
                 const UpscalerInput& in,
                 UpscalerOutput& out) override
    {
        // Deterministic fallback path: preserve the source color when no executable
        // neural upscaler is available in the current runtime.
        out.color = in.color;
    }
};

[[nodiscard]] bool hasExecutablePath(const INeuralRenderer& renderer) noexcept
{
    return renderer.activeDenoiser() != DenoiserBackend::None
        || renderer.activeUpscaler() != UpscalerBackend::None;
}

} // namespace

std::unique_ptr<INeuralRenderer> NeuralRendererFactory::create(
    NeuralBackend backend, nexus::gfx::IDevice& device)
{
    switch (backend) {
    case NeuralBackend::DLSS4:
        return createNeuralRenderer(device, /*preferDLSS=*/true,  /*preferXeSS=*/false, /*preferFSR=*/false);
    case NeuralBackend::DLSS_RR: {
#if defined(NEXUS_BACKEND_VULKAN) && defined(NEXUS_ENABLE_DLSS)
        VkInstance       instance = VK_NULL_HANDLE;
        VkPhysicalDevice physDev  = VK_NULL_HANDLE;
        VkDevice         vkDev    = VK_NULL_HANDLE;
        if (device.backend() == nexus::gfx::Backend::Vulkan) {
            if (auto* vd = dynamic_cast<nexus::gfx::VulkanDevice*>(&device)) {
                instance = vd->instance();
                physDev  = vd->physical();
                vkDev    = vd->logical();
            }
        }
        auto dlssRR = std::make_unique<DLSSPlugin>(instance, physDev, vkDev, /*rrMode=*/true);
        if (dlssRR->available()) return dlssRR;
#endif
        // DLSS SDK absent or non-Vulkan: fall back to the deterministic renderer.
        return std::make_unique<DeterministicFallbackNeuralRenderer>();
    }
    case NeuralBackend::XeSS:
        return createNeuralRenderer(device, /*preferDLSS=*/false, /*preferXeSS=*/true,  /*preferFSR=*/false);
    case NeuralBackend::OIDN_CPU: {
        auto oidn = std::make_unique<OIDNDenoiser>();
        return oidn;
    }
    case NeuralBackend::FSR3: {
        auto fsr = std::make_unique<FSR3Plugin>();
        if (fsr->available()) return fsr;
        // FidelityFX SDK absent: fall back to the deterministic renderer.
        return std::make_unique<DeterministicFallbackNeuralRenderer>();
    }
    case NeuralBackend::Bilinear:
        return std::make_unique<DeterministicFallbackNeuralRenderer>();
    case NeuralBackend::Auto:
    default:
        return createNeuralRenderer(device);
    }
}

std::unique_ptr<INeuralRenderer> createNeuralRenderer(
    nexus::gfx::IDevice& device,
    bool preferDLSS,
    bool preferXeSS,
    bool preferFSR)
{
#ifdef NEXUS_BACKEND_VULKAN
    // Obtain Vulkan handles if the backend is Vulkan.
    VkInstance       instance  = VK_NULL_HANDLE;
    VkPhysicalDevice physDev   = VK_NULL_HANDLE;
    VkDevice         vkDevice  = VK_NULL_HANDLE;

    if (device.backend() == nexus::gfx::Backend::Vulkan) {
        if (auto* vd = dynamic_cast<nexus::gfx::VulkanDevice*>(&device)) {
            instance = vd->instance();
            physDev  = vd->physical();
            vkDevice = vd->logical();
        }
    }
#else
    (void)device;
#endif

#ifdef NEXUS_BACKEND_VULKAN
    // ── DLSS 4 ────────────────────────────────────────────────────────────────
    if (preferDLSS) {
        auto dlss = std::make_unique<DLSSPlugin>(instance, physDev, vkDevice);
        if (dlss->available()) return dlss;
    }

    // ── XeSS ─────────────────────────────────────────────────────────────────
    if (preferXeSS) {
        auto xess = std::make_unique<XeSSPlugin>(vkDevice, physDev);
        if (xess->available()) return xess;
    }
#endif

    // ── FSR 3 (CPU-init GPU upscaler — no Vulkan handle required at init) ────────
    if (preferFSR) {
        auto fsr = std::make_unique<FSR3Plugin>();
        if (fsr->available()) return fsr;
    }

    // ── OIDN (CPU fallback candidate) ─────────────────────────────────────────
    {
        auto oidn = std::make_unique<OIDNDenoiser>();
        if (hasExecutablePath(*oidn)) return oidn;
    }

    // Deterministic baseline renderer for unsupported or unavailable runtimes.
    return std::make_unique<DeterministicFallbackNeuralRenderer>();
}

} // namespace nexus::neural

