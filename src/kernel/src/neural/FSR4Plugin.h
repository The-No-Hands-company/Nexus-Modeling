#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  FSR4Plugin — AMD FidelityFX Super Resolution 4 runtime plugin
//  Loaded at runtime via dlopen/LoadLibrary; gracefully absent if not installed.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/neural/NeuralRenderer.h>

namespace nexus::neural {

class FSR4Plugin final : public INeuralRenderer {
public:
    FSR4Plugin();
    ~FSR4Plugin() override;

    [[nodiscard]] bool available() const noexcept { return m_available; }
    [[nodiscard]] DenoiserBackend activeDenoiser() const noexcept override {
        return m_available ? DenoiserBackend::FSR4 : DenoiserBackend::None;
    }
    [[nodiscard]] UpscalerBackend activeUpscaler() const noexcept override {
        return m_available ? UpscalerBackend::FSR4 : UpscalerBackend::None;
    }

    void denoise(nexus::gfx::CmdBufHandle cmd, const DenoiserInput& in, DenoiserOutput& out) override;
    void upscale(nexus::gfx::CmdBufHandle cmd, const UpscalerInput& in, UpscalerOutput& out) override;

private:
    bool  m_available = false;
    void* m_libHandle = nullptr;  // dlopen handle to FidelityFX 4 SDK shared lib
    void* m_context   = nullptr;  // ffxFsr4UpscalerContextCreate handle

    struct FSR4Pfn {
        void* ContextCreate  = nullptr;  // ffxFsr4UpscalerContextCreate
        void* ContextDestroy = nullptr;  // ffxFsr4UpscalerContextDestroy
        void* Dispatch       = nullptr;  // ffxFsr4UpscalerContextDispatch
    } m_pfn;

    void loadLibrary();
    void initContext();
};

} // namespace nexus::neural
