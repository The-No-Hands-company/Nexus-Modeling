// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Frame capture and Gaussian splat pass extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/FrameCaptureExtension.h>
#include <nexus/render/FrameCaptureExporter.h>
#include <nexus/render/GaussianSplatPass.h>
#include <nexus/gfx/GaussianSplatting.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
fcGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = fcGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = fcGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint64_t uint64Arg(const ScriptCommand& cmd,
                                        std::string_view key, uint64_t def)
{
    if (const auto v = fcGetArg(cmd, key)) {
        uint64_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static nexus::render::RenderPassType parsePassType(std::string_view s)
{
    if (s == "shadow")   return nexus::render::RenderPassType::Shadow;
    if (s == "geometry") return nexus::render::RenderPassType::Geometry;
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

struct FrameCaptureState {
    nexus::render::NullFrameCaptureExporter exporter;

    // Snapshot baseline for capture.snapshot / capture.diff
    nexus::render::FrameCaptureSnapshot     baseline;
    bool                                    hasBaseline = false;

    // Gaussian splat pass
    nexus::render::GaussianSplatPass        splatPass;
    // We keep nodes alive in the state so GaussianSplatPass pointers remain valid
    std::vector<nexus::gfx::GaussianSplatSceneNode> ownedNodes;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerFrameCaptureCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<FrameCaptureState>();

    // ── capture.begin ─────────────────────────────────────────────────────────
    //
    // Starts a new frame capture.
    //
    // Arguments:
    //   frame=<N>   frame index (default 0)
    //
    // On success: "capture.begin ok frame=<N>"
    harness.registry().registerCommand("capture.begin",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint64_t frame = uint64Arg(cmd, "frame", 0u);
            state->exporter.beginCapture(frame);

            messages.push_back("capture.begin ok frame=" + std::to_string(frame));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── capture.record_pass ───────────────────────────────────────────────────
    //
    // Records a pass entry into the current capture.
    //
    // Arguments:
    //   pass=shadow|geometry|composite   (default "composite")
    //   draw_calls=<N>                   (default 0)
    //   triangles=<N>                    (default 0)
    //
    // On success: "capture.record_pass ok pass=<P> draw_calls=<D> triangles=<T>"
    harness.registry().registerCommand("capture.record_pass",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto passStr = fcGetArg(cmd, "pass");
            const nexus::render::RenderPassType pass =
                parsePassType(passStr.value_or("composite"));

            nexus::render::FramePassEntry entry;
            entry.passType   = pass;
            entry.drawCalls  = uintArg(cmd, "draw_calls", 0u);
            entry.triangles  = uintArg(cmd, "triangles", 0u);

            state->exporter.recordPass(entry);

            messages.push_back("capture.record_pass ok"
                " pass="       + std::string(passTypeStr(pass)) +
                " draw_calls=" + std::to_string(entry.drawCalls) +
                " triangles="  + std::to_string(entry.triangles));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── capture.end ───────────────────────────────────────────────────────────
    //
    // Finalises the current frame capture.
    //
    // On success: "capture.end ok total_frames=<F>"
    harness.registry().registerCommand("capture.end",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->exporter.endCapture();

            messages.push_back("capture.end ok"
                " total_frames=" + std::to_string(state->exporter.totalFramesCaptured()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── capture.describe ──────────────────────────────────────────────────────
    //
    // Reports the last completed snapshot.
    //
    // On success: "capture.describe frame=<F> passes=<P> draw_calls=<D> triangles=<T>"
    // Returns false when no snapshot is available yet.
    harness.registry().registerCommand("capture.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->exporter.hasSnapshot()) {
                messages.push_back("capture.describe error: no snapshot yet");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const auto& snap = state->exporter.lastSnapshot();
            messages.push_back("capture.describe"
                " frame="      + std::to_string(snap.frameIndex) +
                " passes="     + std::to_string(snap.passes.size()) +
                " draw_calls=" + std::to_string(snap.totalDrawCalls) +
                " triangles="  + std::to_string(snap.totalTriangles));
            for (size_t i = 0; i < snap.passes.size(); ++i) {
                const auto& p = snap.passes[i];
                messages.push_back("capture.describe pass"
                    " index="      + std::to_string(i) +
                    " type="       + std::string(passTypeStr(p.passType)) +
                    " draw_calls=" + std::to_string(p.drawCalls) +
                    " triangles="  + std::to_string(p.triangles));
            }
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── capture.snapshot ──────────────────────────────────────────────────────
    //
    // Saves the current snapshot as a baseline for later diff.
    // Returns false when no snapshot is available yet.
    //
    // On success: "capture.snapshot ok frame=<F> passes=<P>"
    harness.registry().registerCommand("capture.snapshot",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->exporter.hasSnapshot()) {
                messages.push_back("capture.snapshot error: no snapshot yet");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->baseline    = state->exporter.lastSnapshot();
            state->hasBaseline = true;

            messages.push_back("capture.snapshot ok"
                " frame="  + std::to_string(state->baseline.frameIndex) +
                " passes=" + std::to_string(state->baseline.passes.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── capture.diff ──────────────────────────────────────────────────────────
    //
    // Compares the current last snapshot against the stored baseline.
    // Returns false when no baseline or no current snapshot.
    //
    // On success: "capture.diff ok baseline_frame=<B> current_frame=<C> delta_frames=<D>"
    harness.registry().registerCommand("capture.diff",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasBaseline) {
                messages.push_back("capture.diff error: no baseline");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!state->exporter.hasSnapshot()) {
                messages.push_back("capture.diff error: no current snapshot");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const auto& cur       = state->exporter.lastSnapshot();
            const uint64_t bFrame = state->baseline.frameIndex;
            const uint64_t cFrame = cur.frameIndex;
            const uint64_t delta  = (cFrame >= bFrame) ? (cFrame - bFrame) : 0u;

            messages.push_back("capture.diff ok"
                " baseline_frame=" + std::to_string(bFrame) +
                " current_frame="  + std::to_string(cFrame) +
                " delta_frames="   + std::to_string(delta));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── splat_pass.configure ──────────────────────────────────────────────────
    //
    // Configures the GaussianSplatPass.
    //
    // Arguments:
    //   scale=<f>           splatScaleMultiplier (default 1.0)
    //   sort=none|cpu       sort mode: none → None, cpu → ViewDepthCPU (default "cpu")
    //   opacity_cutoff=<f>  logit opacity cutoff (default -5.5413)
    //
    // On success: "splat_pass.configure ok scale=<S> sort=<M> opacity_cutoff=<O>"
    harness.registry().registerCommand("splat_pass.configure",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            nexus::render::GaussianSplatPassConfig cfg = state->splatPass.config();

            cfg.splatScaleMultiplier = floatArg(cmd, "scale", cfg.splatScaleMultiplier);
            cfg.opacityCutoffLogit   = floatArg(cmd, "opacity_cutoff", cfg.opacityCutoffLogit);

            const auto sortStr = fcGetArg(cmd, "sort");
            if (sortStr) {
                cfg.sortMode = (*sortStr == "none")
                    ? nexus::gfx::SplatSortMode::None
                    : nexus::gfx::SplatSortMode::ViewDepthCPU;
            }

            state->splatPass.setConfig(cfg);

            const std::string sortName =
                (cfg.sortMode == nexus::gfx::SplatSortMode::None) ? "none" : "cpu";

            messages.push_back("splat_pass.configure ok"
                " scale="          + std::to_string(cfg.splatScaleMultiplier) +
                " sort="           + sortName +
                " opacity_cutoff=" + std::to_string(cfg.opacityCutoffLogit));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── splat_pass.attach ─────────────────────────────────────────────────────
    //
    // Creates a GaussianSplatSceneNode from ctx.gaussianCloud and attaches it
    // to the pass.  The node is owned by the shared state so the pointer
    // stays valid for the lifetime of this harness.
    //
    // Returns false when ctx.hasGaussianCloud is false.
    //
    // On success: "splat_pass.attach ok splats=<N> attached=<A>"
    harness.registry().registerCommand("splat_pass.attach",
        [state](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!ctx.hasGaussianCloud) {
                messages.push_back("splat_pass.attach error: no gaussian cloud in context");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->ownedNodes.emplace_back(ctx.gaussianCloud);
            state->splatPass.addCloud(&state->ownedNodes.back());

            messages.push_back("splat_pass.attach ok"
                " splats="   + std::to_string(ctx.gaussianCloud.splatCount()) +
                " attached=" + std::to_string(state->splatPass.attachedCloudCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── splat_pass.clear ──────────────────────────────────────────────────────
    //
    // Removes all attached clouds from the pass and clears owned nodes.
    //
    // On success: "splat_pass.clear ok"
    harness.registry().registerCommand("splat_pass.clear",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->splatPass.clearClouds();
            state->ownedNodes.clear();

            messages.push_back("splat_pass.clear ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── splat_pass.compute_stats ──────────────────────────────────────────────
    //
    // Runs GaussianSplatPass::computeStats() and reports the counters.
    //
    // On success: "splat_pass.compute_stats ok draw_calls=<D> submitted=<S> projected=<P> discarded=<X>"
    harness.registry().registerCommand("splat_pass.compute_stats",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto stats = state->splatPass.computeStats();

            messages.push_back("splat_pass.compute_stats ok"
                " draw_calls=" + std::to_string(stats.splatDrawCalls) +
                " submitted="  + std::to_string(stats.submittedSplats) +
                " projected="  + std::to_string(stats.projectedSplats) +
                " discarded="  + std::to_string(stats.discardedSplats));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── splat_pass.describe ───────────────────────────────────────────────────
    //
    // Reports pass configuration and attached cloud count.
    //
    // On success: "splat_pass.describe clouds=<C> scale=<S> sort=<M>"
    harness.registry().registerCommand("splat_pass.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto& cfg = state->splatPass.config();
            const std::string sortName =
                (cfg.sortMode == nexus::gfx::SplatSortMode::None) ? "none" : "cpu";

            messages.push_back("splat_pass.describe"
                " clouds=" + std::to_string(state->splatPass.attachedCloudCount()) +
                " scale="  + std::to_string(cfg.splatScaleMultiplier) +
                " sort="   + sortName);
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
