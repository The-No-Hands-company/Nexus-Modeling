#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  FSR3Plugin — AMD FidelityFX Super Resolution 3 runtime plugin
//  Loaded at runtime via dlopen/LoadLibrary; gracefully absent if not installed.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/neural/NeuralRenderer.h>

namespace nexus::neural {

class FSR3Plugin final : public INeuralRenderer {
public:
    FSR3Plugin();
    ~FSR3Plugin() override;

    [[nodiscard]] bool available() const noexcept { return m_available; }
    [[nodiscard]] DenoiserBackend activeDenoiser() const noexcept override {
        return m_available ? DenoiserBackend::FSR3 : DenoiserBackend::None;
    }
    [[nodiscard]] UpscalerBackend activeUpscaler() const noexcept override {
        return m_available ? UpscalerBackend::FSR3 : UpscalerBackend::None;
    }

    void denoise(nexus::gfx::CmdBufHandle cmd, const DenoiserInput& in, DenoiserOutput& out) override;
    void upscale(nexus::gfx::CmdBufHandle cmd, const UpscalerInput& in, UpscalerOutput& out) override;

private:
    bool  m_available = false;
    void* m_libHandle = nullptr;  // dlopen handle to FidelityFX SDK shared lib
    void* m_context   = nullptr;  // ffxFsr3UpscalerContextCreate handle

    struct FSR3Pfn {
        void* ContextCreate  = nullptr;  // ffxFsr3UpscalerContextCreate
        void* ContextDestroy = nullptr;  // ffxFsr3UpscalerContextDestroy
        void* Dispatch       = nullptr;  // ffxFsr3UpscalerContextDispatch
    } m_pfn;

    void loadLibrary();
    void initContext();
};

} // namespace nexus::neural
