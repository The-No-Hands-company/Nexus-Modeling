// ─────────────────────────────────────────────────────────────────────────────
//  FSR4Plugin — AMD FidelityFX Super Resolution 4 wrapper
//  CPU-side init; GPU dispatch via the FFX backend interface.
// ─────────────────────────────────────────────────────────────────────────────
#include "FSR4Plugin.h"

#if defined(NEXUS_ENABLE_FSR4)
// Public entry points used here:
//   ffxFsr4UpscalerContextCreate
//   ffxFsr4UpscalerContextDestroy
//   ffxFsr4UpscalerContextDispatch
// Full parameter binding deferred to the Vulkan FFX 4 backend milestone.
#endif

namespace nexus::neural {

FSR4Plugin::FSR4Plugin()
{
#if defined(NEXUS_ENABLE_FSR4)
    loadLibrary();
    if (m_libHandle) initContext();
#endif
}

FSR4Plugin::~FSR4Plugin()
{
#if defined(NEXUS_ENABLE_FSR4)
    if (m_context && m_pfn.ContextDestroy) {
        using PFN = void(*)(void*);
        reinterpret_cast<PFN>(m_pfn.ContextDestroy)(m_context);
        m_context = nullptr;
    }
    if (m_libHandle) {
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(m_libHandle));
#else
        dlclose(m_libHandle);
#endif
        m_libHandle = nullptr;
    }
#endif
}

void FSR4Plugin::loadLibrary()
{
#if defined(NEXUS_ENABLE_FSR4)
#if defined(_WIN32)
    m_libHandle = LoadLibraryA("ffx_fsr4upscaler_x64.dll");
#elif defined(__linux__)
    m_libHandle = dlopen("libffx_fsr4upscaler.so", RTLD_LAZY | RTLD_LOCAL);
#endif
    if (!m_libHandle) return;

#if defined(_WIN32)
#define LOAD_PFN(field, name) \
    m_pfn.field = reinterpret_cast<void*>(GetProcAddress( \
        static_cast<HMODULE>(m_libHandle), name))
#else
#define LOAD_PFN(field, name) \
    m_pfn.field = dlsym(m_libHandle, name)
#endif

    LOAD_PFN(ContextCreate,  "ffxFsr4UpscalerContextCreate");
    LOAD_PFN(ContextDestroy, "ffxFsr4UpscalerContextDestroy");
    LOAD_PFN(Dispatch,       "ffxFsr4UpscalerContextDispatch");

#undef LOAD_PFN
#endif
}

void FSR4Plugin::initContext()
{
#if defined(NEXUS_ENABLE_FSR4)
    if (!m_pfn.ContextCreate) return;
    m_available = true;
#endif
}

void FSR4Plugin::denoise(nexus::gfx::CmdBufHandle /*cmd*/,
                         const DenoiserInput& input, DenoiserOutput& output)
{
    output.color = input.color;
}

void FSR4Plugin::upscale(nexus::gfx::CmdBufHandle /*cmd*/,
                         const UpscalerInput& input, UpscalerOutput& output)
{
#if defined(NEXUS_ENABLE_FSR4)
    if (!m_available || !m_pfn.Dispatch) {
        output.color = input.color;
        return;
    }
    output.color = input.color;
#else
    output.color = input.color;
#endif
}

} // namespace nexus::neural
