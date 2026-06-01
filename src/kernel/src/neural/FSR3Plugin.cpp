// ─────────────────────────────────────────────────────────────────────────────
//  FSR3Plugin — AMD FidelityFX Super Resolution 3 wrapper
//  CPU-side init; GPU dispatch via the FFX backend interface.
// ─────────────────────────────────────────────────────────────────────────────
#include "FSR3Plugin.h"

#if defined(NEXUS_ENABLE_FSR3)
// On platforms where the FidelityFX SDK is compiled in, its headers are pulled
// through the cmake target. The public entry points used here are:
//   ffxFsr3UpscalerContextCreate  (creates the upscaler context)
//   ffxFsr3UpscalerContextDestroy (tears it down)
//   ffxFsr3UpscalerContextDispatch (per-frame upscale dispatch)
// Full parameter binding is deferred until the Vulkan FFX backend integration
// milestone; loadLibrary() / initContext() are provided as scaffolding stubs.
#endif

namespace nexus::neural {

FSR3Plugin::FSR3Plugin()
{
#if defined(NEXUS_ENABLE_FSR3)
    loadLibrary();
    if (m_libHandle) initContext();
#endif
}

FSR3Plugin::~FSR3Plugin()
{
#if defined(NEXUS_ENABLE_FSR3)
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

void FSR3Plugin::loadLibrary()
{
#if defined(NEXUS_ENABLE_FSR3)
#if defined(_WIN32)
    m_libHandle = LoadLibraryA("ffx_fsr3upscaler_x64.dll");
#elif defined(__linux__)
    m_libHandle = dlopen("libffx_fsr3upscaler.so", RTLD_LAZY | RTLD_LOCAL);
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

    LOAD_PFN(ContextCreate,  "ffxFsr3UpscalerContextCreate");
    LOAD_PFN(ContextDestroy, "ffxFsr3UpscalerContextDestroy");
    LOAD_PFN(Dispatch,       "ffxFsr3UpscalerContextDispatch");

#undef LOAD_PFN
#endif
}

void FSR3Plugin::initContext()
{
#if defined(NEXUS_ENABLE_FSR3)
    if (!m_pfn.ContextCreate) return;

    // Full FfxFsr3UpscalerContextDescription parameter wiring is deferred to the
    // Vulkan FFX backend integration milestone. Setting m_available here indicates
    // that the library loaded and the entry points resolved successfully.
    m_available = true;
#endif
}

void FSR3Plugin::denoise(nexus::gfx::CmdBufHandle /*cmd*/,
                         const DenoiserInput& input, DenoiserOutput& output)
{
    // FSR3 temporal accumulation path — full dispatch wiring deferred.
    output.color = input.color;
}

void FSR3Plugin::upscale(nexus::gfx::CmdBufHandle /*cmd*/,
                         const UpscalerInput& input, UpscalerOutput& output)
{
#if defined(NEXUS_ENABLE_FSR3)
    if (!m_available || !m_pfn.Dispatch) {
        output.color = input.color;
        return;
    }
    // Full ffxFsr3UpscalerContextDispatch parameter wiring deferred.
    output.color = input.color;
#else
    output.color = input.color;
#endif
}

} // namespace nexus::neural
