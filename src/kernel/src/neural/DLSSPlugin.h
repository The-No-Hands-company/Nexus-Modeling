#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  DLSSPlugin — NVIDIA DLSS 4 (NGX) runtime plugin
//  Loaded at runtime via dlopen/LoadLibrary; gracefully absent if not installed.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/neural/NeuralRenderer.h>
#include <vulkan/vulkan.h>

namespace nexus::neural {

class DLSSPlugin final : public INeuralRenderer {
public:
    // vkDevice/instance needed for NGX Vulkan init.
    // rrMode=true activates NVSDK_NGX_Feature_RayReconstruction instead of DLSS4.
    DLSSPlugin(VkInstance instance, VkPhysicalDevice physDev, VkDevice device,
               bool rrMode = false);
    ~DLSSPlugin() override;

    [[nodiscard]] bool available() const noexcept { return m_ngxAvailable; }
    [[nodiscard]] DenoiserBackend activeDenoiser() const noexcept override {
        if (!m_ngxAvailable) return DenoiserBackend::None;
        return m_rrMode ? DenoiserBackend::DLSS_RR : DenoiserBackend::DLSS4;
    }
    [[nodiscard]] UpscalerBackend activeUpscaler() const noexcept override {
        return m_ngxAvailable ? UpscalerBackend::DLSS4 : UpscalerBackend::None;
    }

    void denoise(nexus::gfx::CmdBufHandle cmd, const DenoiserInput& in, DenoiserOutput& out) override;
    void upscale(nexus::gfx::CmdBufHandle cmd, const UpscalerInput& in, UpscalerOutput& out) override;

private:
    bool        m_ngxAvailable = false;
    bool        m_rrMode       = false;   // true → Ray Reconstruction feature
    void*       m_libHandle    = nullptr;  // dlopen handle
    void*       m_ngxHandle    = nullptr;  // NVSDK_NGX_Handle
    VkDevice    m_device       = VK_NULL_HANDLE;

    // Function pointer table
    struct NGXPfn {
        void* Init              = nullptr;  // NVSDK_NGX_VULKAN_Init
        void* Shutdown          = nullptr;  // NVSDK_NGX_VULKAN_Shutdown
        void* CreateFeature     = nullptr;  // NVSDK_NGX_VULKAN_CreateFeature
        void* EvaluateFeature   = nullptr;  // NVSDK_NGX_VULKAN_EvaluateFeature
        void* DestroyFeature    = nullptr;  // NVSDK_NGX_VULKAN_DestroyFeature
        void* GetParameters     = nullptr;  // NVSDK_NGX_VULKAN_GetCapabilityParameters
        void* AllocParameters   = nullptr;  // NVSDK_NGX_AllocateParameters
        void* DestroyParameters = nullptr;  // NVSDK_NGX_DestroyParameters
        void* SetParameterULL   = nullptr;  // NVSDK_NGX_Parameter_SetULL
        void* SetParameterF     = nullptr;  // NVSDK_NGX_Parameter_SetF
        void* SetParameterVoidP = nullptr;  // NVSDK_NGX_Parameter_SetVoidPointer
    } m_pfn;

    void* m_featureHandle    = nullptr;  // NVSDK_NGX_Handle* for the active feature
    void* m_parametersHandle = nullptr;  // NVSDK_NGX_Parameter* for per-eval params

    void loadLibrary();
    void initNGX(VkInstance instance, VkPhysicalDevice physDev, VkDevice device);
    void createFeatureHandle(VkCommandBuffer cmd);
    void evaluateFeature(VkCommandBuffer cmd, void* params) const;
};

} // namespace nexus::neural
