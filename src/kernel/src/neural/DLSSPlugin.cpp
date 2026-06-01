// ─────────────────────────────────────────────────────────────────────────────
//  DLSSPlugin — NVIDIA DLSS 4 runtime wrapper
//  All NGX calls routed through runtime function pointers loaded from the SDK.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef NEXUS_BACKEND_VULKAN
#include "DLSSPlugin.h"
#include <cstring>

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   define NEXUS_DLOPEN(path)     LoadLibraryA(path)
#   define NEXUS_DLSYM(h, sym)    GetProcAddress(static_cast<HMODULE>(h), sym)
#   define NEXUS_DLCLOSE(h)       FreeLibrary(static_cast<HMODULE>(h))
#   define DLSS_LIB_NAME          "nvngx_dlss.dll"
#else
#   include <dlfcn.h>
#   define NEXUS_DLOPEN(path)     dlopen(path, RTLD_LAZY | RTLD_LOCAL)
#   define NEXUS_DLSYM(h, sym)    dlsym(h, sym)
#   define NEXUS_DLCLOSE(h)       dlclose(h)
#   define DLSS_LIB_NAME          "libnvsdk_ngx.so.1"
#   define DLSS_LIB_NAME_ALT      "libnvsdk_ngx.so"
#endif

namespace nexus::neural {

DLSSPlugin::DLSSPlugin(VkInstance instance, VkPhysicalDevice physDev, VkDevice device,
                       bool rrMode)
    : m_device(device), m_rrMode(rrMode)
{
#if defined(NEXUS_ENABLE_DLSS)
    loadLibrary();
    if (m_libHandle) initNGX(instance, physDev, device);
#else
    (void)instance; (void)physDev; (void)device;
#endif
}

DLSSPlugin::~DLSSPlugin()
{
#if defined(NEXUS_ENABLE_DLSS)
    if (m_featureHandle && m_pfn.DestroyFeature) {
        using Fn = void(*)(void*);
        reinterpret_cast<Fn>(m_pfn.DestroyFeature)(m_featureHandle);
    }
    if (m_parametersHandle && m_pfn.DestroyParameters) {
        using Fn = void(*)(void*);
        reinterpret_cast<Fn>(m_pfn.DestroyParameters)(m_parametersHandle);
    }
    if (m_ngxHandle && m_pfn.DestroyFeature) {
        using Fn = void(*)(void*);
        reinterpret_cast<Fn>(m_pfn.DestroyFeature)(m_ngxHandle);
    }
    if (m_pfn.Shutdown) {
        using Fn = void(*)(VkDevice);
        reinterpret_cast<Fn>(m_pfn.Shutdown)(m_device);
    }
    if (m_libHandle) NEXUS_DLCLOSE(m_libHandle);
#endif
}

void DLSSPlugin::loadLibrary()
{
    m_libHandle = NEXUS_DLOPEN(DLSS_LIB_NAME);
#if !defined(_WIN32) && defined(DLSS_LIB_NAME_ALT)
    if (!m_libHandle) m_libHandle = NEXUS_DLOPEN(DLSS_LIB_NAME_ALT);
#endif
    if (!m_libHandle) return;

    m_pfn.Init              = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_Init");
    m_pfn.Shutdown          = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_Shutdown");
    m_pfn.CreateFeature     = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_CreateFeature");
    m_pfn.EvaluateFeature   = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_EvaluateFeature");
    m_pfn.DestroyFeature    = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_DestroyFeature");
    m_pfn.GetParameters     = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_VULKAN_GetCapabilityParameters");
    m_pfn.AllocParameters   = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_AllocateParameters");
    m_pfn.DestroyParameters = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_DestroyParameters");
    m_pfn.SetParameterULL   = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_Parameter_SetULL");
    m_pfn.SetParameterF     = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_Parameter_SetF");
    m_pfn.SetParameterVoidP = NEXUS_DLSYM(m_libHandle, "NVSDK_NGX_Parameter_SetVoidPointer");
}

void DLSSPlugin::initNGX(VkInstance instance, VkPhysicalDevice physDev, VkDevice device)
{
    if (!m_pfn.Init) return;

    // NVSDK_NGX_VULKAN_Init signature (simplified):
    //   NVSDK_NGX_Result NVSDK_NGX_VULKAN_Init(
    //       unsigned long long AppId, const wchar_t* LogPath,
    //       VkInstance, VkPhysicalDevice, VkDevice,
    //       PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr,
    //       const NVSDK_NGX_FeatureCommonInfo*, NVSDK_NGX_Version);
    using InitFn = int(*)(unsigned long long, const wchar_t*,
                          VkInstance, VkPhysicalDevice, VkDevice,
                          PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr,
                          const void*, int);

    const int NVSDK_NGX_Version_API = 0x0000014;  // 1.4
    int result = reinterpret_cast<InitFn>(m_pfn.Init)(
        0x4E657875736D6F64ull,  // "Nexusmod" project ID placeholder
        nullptr,
        instance, physDev, device,
        vkGetInstanceProcAddr, vkGetDeviceProcAddr,
        nullptr, NVSDK_NGX_Version_API);

    m_ngxAvailable = (result == 1 /*NVSDK_NGX_Result_Success*/);
    if (!m_ngxAvailable) return;

    // Allocate the per-evaluation NGX parameter block.
    if (m_pfn.AllocParameters) {
        using AllocFn = int(*)(void**);
        void* params = nullptr;
        if (reinterpret_cast<AllocFn>(m_pfn.AllocParameters)(&params) == 1)
            m_parametersHandle = params;
    }
}

void DLSSPlugin::createFeatureHandle(VkCommandBuffer cmd)
{
    if (!m_pfn.CreateFeature || !m_parametersHandle) return;

    // NVSDK_NGX_VULKAN_CreateFeature signature (simplified):
    //   NVSDK_NGX_Result NVSDK_NGX_VULKAN_CreateFeature(
    //       VkCommandBuffer, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
    // NVSDK_NGX_Feature_DLSS = 1, NVSDK_NGX_Feature_RayReconstruction = 1000
    const int featureId = m_rrMode ? 1000 : 1;
    using CreateFn = int(*)(VkCommandBuffer, int, void*, void**);
    void* handle = nullptr;
    const int r = reinterpret_cast<CreateFn>(m_pfn.CreateFeature)(
        cmd, featureId, m_parametersHandle, &handle);
    if (r == 1) m_featureHandle = handle;
}

void DLSSPlugin::evaluateFeature(VkCommandBuffer cmd, void* params) const
{
    if (!m_pfn.EvaluateFeature || !m_featureHandle) return;
    // NVSDK_NGX_VULKAN_EvaluateFeature(VkCommandBuffer, NVSDK_NGX_Handle*, NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback)
    using EvalFn = int(*)(VkCommandBuffer, void*, void*, void*);
    reinterpret_cast<EvalFn>(m_pfn.EvaluateFeature)(cmd, m_featureHandle, params, nullptr);
}

void DLSSPlugin::upscale(nexus::gfx::CmdBufHandle cmd, const UpscalerInput& input, UpscalerOutput& output)
{
    if (!m_ngxAvailable || !m_pfn.EvaluateFeature) {
        output.color = input.color;
        return;
    }

    // Obtain a raw Vulkan command buffer handle from the opaque CmdBufHandle.
    // The CmdBufHandle id field carries the VkCommandBuffer cast as uint64_t
    // on Vulkan backends; safe to use here because this code is already behind
    // #ifdef NEXUS_BACKEND_VULKAN.
    auto vkCmd = reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(cmd.id));

    if (!m_featureHandle) createFeatureHandle(vkCmd);
    if (!m_featureHandle || !m_parametersHandle) {
        output.color = input.color;
        return;
    }

    void* params = m_parametersHandle;

    // Populate NGX upscaler input parameters via the SetParameter entry points.
    // Parameter name constants follow the NVSDK_NGX_Parameter_* convention.
    if (m_pfn.SetParameterULL) {
        using SetULLFn = void(*)(void*, const char*, unsigned long long);
        auto setULL = reinterpret_cast<SetULLFn>(m_pfn.SetParameterULL);
        setULL(params, "Color",         static_cast<unsigned long long>(input.color.id));
        setULL(params, "Depth",         static_cast<unsigned long long>(input.depth.id));
        setULL(params, "MotionVectors", static_cast<unsigned long long>(input.motionVectors.id));
        setULL(params, "Output",        static_cast<unsigned long long>(output.color.id));
        setULL(params, "RenderSizeX",   input.renderResolution.width);
        setULL(params, "RenderSizeY",   input.renderResolution.height);
        setULL(params, "OutputSizeX",   input.outputResolution.width);
        setULL(params, "OutputSizeY",   input.outputResolution.height);
    }
    if (m_pfn.SetParameterF) {
        using SetFFn = void(*)(void*, const char*, float);
        auto setF = reinterpret_cast<SetFFn>(m_pfn.SetParameterF);
        setF(params, "JitterOffsetX", input.jitterX);
        setF(params, "JitterOffsetY", input.jitterY);
        setF(params, "Reset",         input.reset ? 1.f : 0.f);
    }

    evaluateFeature(vkCmd, params);

    // Upscaler writes result into the output texture in place — handle unchanged.
    output.color = input.color;
}

void DLSSPlugin::denoise(nexus::gfx::CmdBufHandle cmd, const DenoiserInput& input, DenoiserOutput& output)
{
    if (!m_ngxAvailable || !m_pfn.EvaluateFeature) {
        output.color = input.color;
        return;
    }

    auto vkCmd = reinterpret_cast<VkCommandBuffer>(static_cast<uintptr_t>(cmd.id));

    if (!m_featureHandle) createFeatureHandle(vkCmd);
    if (!m_featureHandle || !m_parametersHandle) {
        output.color = input.color;
        return;
    }

    void* params = m_parametersHandle;

    if (m_pfn.SetParameterULL) {
        using SetULLFn = void(*)(void*, const char*, unsigned long long);
        auto setULL = reinterpret_cast<SetULLFn>(m_pfn.SetParameterULL);
        setULL(params, "Color",         static_cast<unsigned long long>(input.color.id));
        setULL(params, "Albedo",        static_cast<unsigned long long>(input.albedo.id));
        setULL(params, "Normal",        static_cast<unsigned long long>(input.normal.id));
        setULL(params, "MotionVectors", static_cast<unsigned long long>(input.motionVectors.id));
        setULL(params, "Output",        static_cast<unsigned long long>(output.color.id));
    }
    if (m_pfn.SetParameterF) {
        using SetFFn = void(*)(void*, const char*, float);
        reinterpret_cast<SetFFn>(m_pfn.SetParameterF)(
            params, "ExposureScale", input.exposureScale);
    }

    // m_rrMode selects NVSDK_NGX_Feature_RayReconstruction (featureId=1000) vs
    // standard DLSS4 denoiser (featureId=1) — the createFeatureHandle call above
    // already encoded the correct feature ID when the handle was first created.
    evaluateFeature(vkCmd, params);

    output.color = input.color;
}

} // namespace nexus::neural
#endif // NEXUS_BACKEND_VULKAN
