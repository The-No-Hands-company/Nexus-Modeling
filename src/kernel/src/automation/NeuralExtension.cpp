// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Neural renderer extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/NeuralExtension.h>
#include <nexus/neural/NeuralRenderer.h>
#include <nexus/gfx/Device.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
neuralGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static const char* denoiserStr(nexus::neural::DenoiserBackend b)
{
    switch (b) {
        case nexus::neural::DenoiserBackend::None:     return "none";
        case nexus::neural::DenoiserBackend::OIDN_CPU: return "oidn_cpu";
        case nexus::neural::DenoiserBackend::DLSS4:    return "dlss4";
        case nexus::neural::DenoiserBackend::DLSS_RR:  return "dlss_rr";
        case nexus::neural::DenoiserBackend::XeSS:     return "xess";
        case nexus::neural::DenoiserBackend::FSR3:     return "fsr3";
        case nexus::neural::DenoiserBackend::FSR4:     return "fsr4";
    }
    return "none";
}

[[nodiscard]] static const char* upscalerStr(nexus::neural::UpscalerBackend b)
{
    switch (b) {
        case nexus::neural::UpscalerBackend::None:     return "none";
        case nexus::neural::UpscalerBackend::Bilinear: return "bilinear";
        case nexus::neural::UpscalerBackend::DLSS4:    return "dlss4";
        case nexus::neural::UpscalerBackend::XeSS:     return "xess";
        case nexus::neural::UpscalerBackend::FSR3:     return "fsr3";
        case nexus::neural::UpscalerBackend::FSR4:     return "fsr4";
    }
    return "none";
}

struct NeuralState {
    std::unique_ptr<nexus::gfx::IDevice>       device;
    std::unique_ptr<nexus::neural::INeuralRenderer> renderer;
    bool hasRenderer = false;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerNeuralCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<NeuralState>();

    // ── neural.init ───────────────────────────────────────────────────────────
    //
    // Creates a neural renderer using the Null device backend.
    // Re-initialising is allowed and replaces the current instance.
    //
    // Arguments:
    //   prefer_dlss=0|1   (default 1)
    //   prefer_xess=0|1   (default 1)
    //   prefer_fsr=0|1    (default 1)
    //
    // On success: "neural.init ok denoiser=<D> upscaler=<U>"
    // On error:   "neural.init error: ..."
    harness.registry().registerCommand("neural.init",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto pDlss = neuralGetArg(cmd, "prefer_dlss");
            const auto pXess = neuralGetArg(cmd, "prefer_xess");
            const auto pFsr  = neuralGetArg(cmd, "prefer_fsr");

            const bool dlss = (!pDlss || *pDlss != "0");
            const bool xess = (!pXess || *pXess != "0");
            const bool fsr  = (!pFsr  || *pFsr  != "0");

            state->device   = nexus::gfx::createDevice(nexus::gfx::Backend::Null);
            state->renderer = nexus::neural::createNeuralRenderer(
                *state->device, dlss, xess, fsr);

            if (!state->renderer) {
                messages.push_back("neural.init error: renderer creation failed");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->hasRenderer = true;
            messages.push_back("neural.init ok"
                " denoiser=" + std::string(denoiserStr(state->renderer->activeDenoiser())) +
                " upscaler=" + std::string(upscalerStr(state->renderer->activeUpscaler())));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── neural.describe ───────────────────────────────────────────────────────
    //
    // Reports the active denoiser and upscaler backends.
    // Returns false when no renderer has been initialised.
    //
    // "neural.describe denoiser=<D> upscaler=<U>"
    harness.registry().registerCommand("neural.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasRenderer) {
                messages.push_back("neural.describe error: not initialised");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("neural.describe"
                " denoiser=" + std::string(denoiserStr(state->renderer->activeDenoiser())) +
                " upscaler=" + std::string(upscalerStr(state->renderer->activeUpscaler())));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
