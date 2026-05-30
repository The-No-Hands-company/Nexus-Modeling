// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Renderer settings / stats / validation extension
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/RendererExtension.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
rnGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = rnGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = rnGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static nexus::render::RenderMode parseRenderMode(std::string_view s)
{
    if (s == "PathTrace")  return nexus::render::RenderMode::PathTrace;
    if (s == "HybridRT")   return nexus::render::RenderMode::HybridRT;
    if (s == "Wireframe")  return nexus::render::RenderMode::Wireframe;
    if (s == "Normals")    return nexus::render::RenderMode::Normals;
    if (s == "Overdraw")   return nexus::render::RenderMode::Overdraw;
    return nexus::render::RenderMode::Rasterize;
}

[[nodiscard]] static nexus::render::UpscaleMode parseUpscaleMode(std::string_view s)
{
    if (s == "DLSS4")    return nexus::render::UpscaleMode::DLSS4;
    if (s == "XeSS")     return nexus::render::UpscaleMode::XeSS;
    if (s == "FSR3")     return nexus::render::UpscaleMode::FSR3;
    if (s == "Bilinear") return nexus::render::UpscaleMode::Bilinear;
    return nexus::render::UpscaleMode::Off;
}

[[nodiscard]] static std::string renderModeName(nexus::render::RenderMode m)
{
    switch (m) {
        case nexus::render::RenderMode::Rasterize: return "Rasterize";
        case nexus::render::RenderMode::PathTrace:  return "PathTrace";
        case nexus::render::RenderMode::HybridRT:   return "HybridRT";
        case nexus::render::RenderMode::Wireframe:  return "Wireframe";
        case nexus::render::RenderMode::Normals:    return "Normals";
        case nexus::render::RenderMode::Overdraw:   return "Overdraw";
    }
    return "Unknown";
}

[[nodiscard]] static std::string upscaleModeName(nexus::render::UpscaleMode u)
{
    switch (u) {
        case nexus::render::UpscaleMode::Off:      return "Off";
        case nexus::render::UpscaleMode::DLSS4:    return "DLSS4";
        case nexus::render::UpscaleMode::XeSS:     return "XeSS";
        case nexus::render::UpscaleMode::FSR3:     return "FSR3";
        case nexus::render::UpscaleMode::Bilinear: return "Bilinear";
    }
    return "Unknown";
}

struct RendererState {
    std::unique_ptr<nexus::gfx::RenderContext> ctx;
    std::unique_ptr<nexus::gfx::ISwapchain>    swapchain;
    std::unique_ptr<nexus::render::Renderer>   renderer;

    RendererState()
    {
        nexus::gfx::RenderContextDesc desc{};
        desc.preferredBackend = nexus::gfx::Backend::Null;
        desc.validation       = nexus::gfx::ValidationLevel::Off;
        ctx = nexus::gfx::RenderContext::create(desc);

        nexus::gfx::SwapchainDesc sd{};
        sd.extent = {1280u, 720u};
        swapchain = ctx->createSwapchain(sd);

        renderer = std::make_unique<nexus::render::Renderer>(*ctx, *swapchain);
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerRendererCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<RendererState>();

    // ── renderer.settings.set ─────────────────────────────────────────────────
    //
    // Applies renderer settings.  Silently downgrades unsupported modes on the
    // null backend (e.g. PathTrace → Rasterize).
    //
    // Arguments (all optional):
    //   mode=Rasterize|PathTrace|HybridRT|Wireframe|Normals|Overdraw
    //   upscale=Off|DLSS4|XeSS|FSR3|Bilinear
    //   scale=<f>         render scale (default 1.0)
    //   bounces=<N>       max PT bounces (default 4)
    //   taa=0|1           TAA on/off
    //
    // On success: "renderer.settings.set ok mode=<M> upscale=<U>"
    harness.registry().registerCommand("renderer.settings.set",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            nexus::render::RendererSettings s = state->renderer->settings();

            if (const auto v = rnGetArg(cmd, "mode"))
                s.mode = parseRenderMode(*v);
            if (const auto v = rnGetArg(cmd, "upscale"))
                s.upscale = parseUpscaleMode(*v);
            const float scale = floatArg(cmd, "scale", s.renderScale);
            s.renderScale = scale;
            const uint32_t bounces = uintArg(cmd, "bounces", s.maxBounces);
            s.maxBounces = bounces;
            if (const auto v = rnGetArg(cmd, "taa"))
                s.enableTAA = (*v != "0");

            state->renderer->applySettings(s);

            const auto& applied = state->renderer->settings();
            messages.push_back("renderer.settings.set ok"
                " mode="    + renderModeName(applied.mode) +
                " upscale=" + upscaleModeName(applied.upscale));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── renderer.settings.describe ────────────────────────────────────────────
    //
    // Reports current renderer settings.  Always returns true.
    //
    // "renderer.settings.describe mode=<M> upscale=<U> scale=<S> taa=<0|1>"
    harness.registry().registerCommand("renderer.settings.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto& s = state->renderer->settings();
            messages.push_back("renderer.settings.describe"
                " mode="    + renderModeName(s.mode) +
                " upscale=" + upscaleModeName(s.upscale) +
                " scale="   + std::to_string(s.renderScale) +
                " taa="     + std::string(s.enableTAA ? "1" : "0"));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── renderer.frame.run ────────────────────────────────────────────────────
    //
    // Runs beginFrame / render / endFrame with default camera and empty scene.
    //
    // On success: "renderer.frame.run ok"
    harness.registry().registerCommand("renderer.frame.run",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            nexus::render::Camera cam;
            cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 1000.f);
            cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});
            nexus::render::SceneGraph sg;

            state->renderer->beginFrame();
            state->renderer->render(cam, sg);
            state->renderer->endFrame();

            messages.push_back("renderer.frame.run ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── renderer.frame_stats.describe ────────────────────────────────────────
    //
    // Reports last frame stats.  Always returns true.
    //
    // "renderer.frame_stats.describe nodes=<N> draws=<D> gpu_ms=<G>"
    harness.registry().registerCommand("renderer.frame_stats.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto& fs = state->renderer->lastFrameStats();
            messages.push_back("renderer.frame_stats.describe"
                " nodes=" + std::to_string(fs.totalNodes) +
                " draws=" + std::to_string(fs.drawCalls) +
                " gpu_ms=" + std::to_string(fs.gpuTimeMs));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── renderer.validation.enable ───────────────────────────────────────────
    //
    // Enables render graph validation.
    //
    // On success: "renderer.validation.enable ok"
    harness.registry().registerCommand("renderer.validation.enable",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->renderer->setRenderGraphValidationEnabled(true);
            messages.push_back("renderer.validation.enable ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── renderer.validation.disable ──────────────────────────────────────────
    //
    // Disables render graph validation.
    //
    // On success: "renderer.validation.disable ok"
    harness.registry().registerCommand("renderer.validation.disable",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->renderer->setRenderGraphValidationEnabled(false);
            messages.push_back("renderer.validation.disable ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── renderer.validation.describe ─────────────────────────────────────────
    //
    // Reports validation enabled state and last report valid flag.
    // Always returns true.
    //
    // "renderer.validation.describe enabled=<0|1> valid=<0|1> issues=<N>"
    harness.registry().registerCommand("renderer.validation.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const bool enabled = state->renderer->isRenderGraphValidationEnabled();
            const auto& report = state->renderer->lastRenderGraphReport();
            messages.push_back("renderer.validation.describe"
                " enabled=" + std::string(enabled ? "1" : "0") +
                " valid="   + std::string(report.valid ? "1" : "0") +
                " issues="  + std::to_string(report.issues.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── renderer.resize ───────────────────────────────────────────────────────
    //
    // Notifies the renderer of a swapchain resize.
    //
    // Arguments:
    //   width=<N>   new width  (default 1280)
    //   height=<N>  new height (default 720)
    //
    // On success: "renderer.resize ok width=<W> height=<H>"
    harness.registry().registerCommand("renderer.resize",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint32_t w = uintArg(cmd, "width",  1280u);
            const uint32_t h = uintArg(cmd, "height",  720u);
            state->renderer->onResize({w, h});
            messages.push_back("renderer.resize ok"
                " width="  + std::to_string(w) +
                " height=" + std::to_string(h));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
