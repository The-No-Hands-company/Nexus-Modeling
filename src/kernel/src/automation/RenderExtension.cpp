// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Render pipeline extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/RenderExtension.h>
#include <nexus/render/RenderGraphValidator.h>
#include <nexus/render/TemporalAccumulation.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
renderGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = renderGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = renderGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static nexus::render::RenderPassType parsePassType(std::string_view s)
{
    if (s == "shadow")    return nexus::render::RenderPassType::Shadow;
    if (s == "geometry")  return nexus::render::RenderPassType::Geometry;
    return nexus::render::RenderPassType::Composite;
}

[[nodiscard]] static const char* passTypeStr(nexus::render::RenderPassType t)
{
    switch (t) {
        case nexus::render::RenderPassType::Shadow:    return "shadow";
        case nexus::render::RenderPassType::Geometry:  return "geometry";
        case nexus::render::RenderPassType::Composite: return "composite";
    }
    return "composite";
}

[[nodiscard]] static nexus::gfx::TextureLayout parseLayout(std::string_view s)
{
    if (s == "color_attachment")  return nexus::gfx::TextureLayout::ColorAttachment;
    if (s == "shader_read")       return nexus::gfx::TextureLayout::ShaderRead;
    if (s == "depth_write")       return nexus::gfx::TextureLayout::DepthWrite;
    if (s == "depth_read")        return nexus::gfx::TextureLayout::DepthRead;
    if (s == "present")           return nexus::gfx::TextureLayout::Present;
    return nexus::gfx::TextureLayout::Undefined;
}

[[nodiscard]] static const char* layoutStr(nexus::gfx::TextureLayout l)
{
    switch (l) {
        case nexus::gfx::TextureLayout::Undefined:        return "undefined";
        case nexus::gfx::TextureLayout::ColorAttachment:  return "color_attachment";
        case nexus::gfx::TextureLayout::ShaderRead:       return "shader_read";
        case nexus::gfx::TextureLayout::DepthWrite:       return "depth_write";
        case nexus::gfx::TextureLayout::DepthRead:        return "depth_read";
        case nexus::gfx::TextureLayout::Present:          return "present";
        case nexus::gfx::TextureLayout::General:          return "general";
        case nexus::gfx::TextureLayout::ShaderReadWrite:  return "shader_read_write";
        case nexus::gfx::TextureLayout::TransferSrc:      return "transfer_src";
        case nexus::gfx::TextureLayout::TransferDst:      return "transfer_dst";
    }
    return "undefined";
}

struct RenderState {
    nexus::render::RenderGraphDesc      graphDesc;
    nexus::render::TemporalAccumulator  taa;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerRenderCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<RenderState>();

    // ── render.graph.record ───────────────────────────────────────────────────
    //
    // Appends a render pass entry to the local RenderGraphDesc.
    //
    // Arguments:
    //   pass=shadow|geometry|composite        (default "composite")
    //   gbuffer=<layout>                      GBuffer color layout (default "undefined")
    //   shadow_atlas=<layout>                 Shadow atlas layout  (default "undefined")
    //
    // Layout tokens: undefined, color_attachment, shader_read,
    //                depth_write, depth_read, present
    //
    // On success: "render.graph.record ok pass=<P> index=<I>"
    harness.registry().registerCommand("render.graph.record",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto passStr = renderGetArg(cmd, "pass");
            const nexus::render::RenderPassType pass =
                parsePassType(passStr.value_or("composite"));

            const nexus::gfx::TextureLayout gbuf =
                parseLayout(renderGetArg(cmd, "gbuffer").value_or("undefined"));
            const nexus::gfx::TextureLayout shadow =
                parseLayout(renderGetArg(cmd, "shadow_atlas").value_or("undefined"));

            const size_t idx = state->graphDesc.passes.size();
            state->graphDesc.record(pass, gbuf, shadow);

            messages.push_back("render.graph.record ok"
                " pass="  + std::string(passTypeStr(pass)) +
                " index=" + std::to_string(idx));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── render.graph.clear ────────────────────────────────────────────────────
    //
    // Resets the local RenderGraphDesc to empty.
    //
    // On success: "render.graph.clear ok"
    harness.registry().registerCommand("render.graph.clear",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->graphDesc.passes.clear();
            messages.push_back("render.graph.clear ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── render.graph.validate ─────────────────────────────────────────────────
    //
    // Runs RenderGraphValidator::validate() on the current graph desc.
    // Returns true when the graph is valid; false when issues are found.
    //
    // On success: "render.graph.validate ok passes=<N>"
    // On issues:  "render.graph.validate fail issues=<K>" + per-issue lines
    harness.registry().registerCommand("render.graph.validate",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto report =
                nexus::render::RenderGraphValidator::validate(state->graphDesc);

            if (report.valid) {
                messages.push_back("render.graph.validate ok"
                    " passes=" + std::to_string(state->graphDesc.passes.size()));
                std::sort(messages.begin(), messages.end());
                return true;
            }

            messages.push_back("render.graph.validate fail"
                " issues=" + std::to_string(report.issues.size()));
            for (const auto& iss : report.issues)
                messages.push_back("render.graph.validate issue: " + iss.message);
            std::sort(messages.begin(), messages.end());
            return false;
        });

    // ── render.graph.describe ─────────────────────────────────────────────────
    //
    // Reports the current graph desc. Always returns true.
    harness.registry().registerCommand("render.graph.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("render.graph.describe passes=" +
                std::to_string(state->graphDesc.passes.size()));
            for (size_t i = 0; i < state->graphDesc.passes.size(); ++i) {
                const auto& p = state->graphDesc.passes[i];
                messages.push_back("render.graph.describe pass"
                    " index="        + std::to_string(i) +
                    " type="         + std::string(passTypeStr(p.passType)) +
                    " gbuffer="      + std::string(layoutStr(p.gbufferColorLayout)) +
                    " shadow_atlas=" + std::string(layoutStr(p.shadowAtlasLayout)));
            }
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── render.taa.configure ──────────────────────────────────────────────────
    //
    // Configures the TemporalAccumulator.
    //
    // Arguments:
    //   blend=<f>       blend factor 0.0–1.0 (default 0.1)
    //   samples=<N>     jitter sample count (default 8)
    //   jitter=halton|uniform   (default "halton")
    //
    // On success: "render.taa.configure ok blend=<B> samples=<S> jitter=<J>"
    harness.registry().registerCommand("render.taa.configure",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            nexus::render::TemporalAccumulationConfig cfg = state->taa.config();

            cfg.blendFactor = floatArg(cmd, "blend", cfg.blendFactor);

            const uint32_t samples = uintArg(cmd, "samples",
                cfg.jitter.sampleCount);
            cfg.jitter.sampleCount = samples;

            const auto jitterStr = renderGetArg(cmd, "jitter");
            if (jitterStr) {
                cfg.jitter.type = (*jitterStr == "uniform")
                    ? nexus::render::JitterPattern::kUniform
                    : nexus::render::JitterPattern::kHalton;
            }

            state->taa.setConfig(cfg);

            const std::string jitterName =
                (cfg.jitter.type == nexus::render::JitterPattern::kUniform)
                    ? "uniform" : "halton";

            messages.push_back("render.taa.configure ok"
                " blend="   + std::to_string(cfg.blendFactor) +
                " samples=" + std::to_string(cfg.jitter.sampleCount) +
                " jitter="  + jitterName);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── render.taa.advance ────────────────────────────────────────────────────
    //
    // Advances the TemporalAccumulator by one frame.
    //
    // Arguments:
    //   frames=<N>   number of frames to advance (default 1)
    //
    // On success: "render.taa.advance ok frame=<F>"
    harness.registry().registerCommand("render.taa.advance",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint32_t frames = uintArg(cmd, "frames", 1u);
            for (uint32_t i = 0; i < frames; ++i)
                state->taa.advanceFrame();

            messages.push_back("render.taa.advance ok"
                " frame=" + std::to_string(state->taa.state().frameIndex));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── render.taa.describe ───────────────────────────────────────────────────
    //
    // Reports current TAA state. Always returns true.
    //
    // "render.taa.describe frame=<F> blend=<B> jitter_x=<X> jitter_y=<Y>"
    harness.registry().registerCommand("render.taa.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto [jx, jy] = state->taa.currentJitterOffset();
            const float blend   = state->taa.resolveBlendAlpha();

            messages.push_back("render.taa.describe"
                " frame="    + std::to_string(state->taa.state().frameIndex) +
                " blend="    + std::to_string(blend) +
                " jitter_x=" + std::to_string(jx) +
                " jitter_y=" + std::to_string(jy));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
