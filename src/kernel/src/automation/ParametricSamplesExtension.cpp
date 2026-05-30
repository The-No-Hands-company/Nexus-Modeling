// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Parametric sample generator extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/ParametricSamplesExtension.h>
#include <nexus/parametric/ParametricSamples.h>
#include <nexus/parametric/ParametricSolver.h>
#include <nexus/parametric/ConstraintGraph.h>

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
psGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static double doubleArg(const ScriptCommand& cmd,
                                      std::string_view key, double def)
{
    if (const auto v = psGetArg(cmd, key)) {
        double out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = psGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

struct ParamSamplesState {
    std::optional<nexus::parametric::SketchSampleModel> model;
    bool hasModel     = false;
    bool lastConverged = false;
    uint32_t lastIterations = 0;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerParametricSamplesCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<ParamSamplesState>();

    // ── param_samples.make_rect ───────────────────────────────────────────────
    //
    // Constructs a SketchSampleModel representing a rectangle constraint sketch.
    //
    // Arguments:
    //   width=<f>    rectangle width  (default 1.0)
    //   height=<f>   rectangle height (default 1.0)
    //
    // On success: "param_samples.make_rect ok width=<W> height=<H> entities=<E>"
    harness.registry().registerCommand("param_samples.make_rect",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const double w = doubleArg(cmd, "width",  1.0);
            const double h = doubleArg(cmd, "height", 1.0);

            state->model = nexus::parametric::ParametricSampleGenerator
                ::makeSketchRectangle(w, h);
            state->hasModel      = true;
            state->lastConverged = false;
            state->lastIterations = 0;

            const size_t entities = state->model->graph.entityCount();

            messages.push_back("param_samples.make_rect ok"
                " width="    + std::to_string(w) +
                " height="   + std::to_string(h) +
                " entities=" + std::to_string(entities));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── param_samples.solve ───────────────────────────────────────────────────
    //
    // Runs the parametric solver on the current SketchSampleModel.
    // Returns false when no model is loaded or solver did not converge.
    //
    // Arguments:
    //   max_iter=<N>    maximum solver iterations (default 50)
    //   tolerance=<f>   convergence tolerance     (default 1e-6)
    //
    // On success:  "param_samples.solve ok converged=1 iterations=<I>"
    // On no-model: "param_samples.solve error: no model"
    // On fail:     "param_samples.solve ok converged=0 iterations=<I>"
    //              (still returns true — partial result is informational)
    harness.registry().registerCommand("param_samples.solve",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasModel) {
                messages.push_back("param_samples.solve error: no model");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::parametric::ParametricSolverConfig cfg;
            cfg.maxIterations = uintArg(cmd, "max_iter", 50u);

            const auto report =
                nexus::parametric::ParametricSampleGenerator
                    ::solveSketchRectangle(*state->model, cfg);

            state->lastConverged  = report.converged;
            state->lastIterations = report.iterationsRan;

            messages.push_back("param_samples.solve ok"
                " converged="  + std::string(report.converged ? "1" : "0") +
                " iterations=" + std::to_string(report.iterationsRan));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── param_samples.describe ────────────────────────────────────────────────
    //
    // Reports model state and last solver result.  Always returns true.
    //
    // "param_samples.describe has_model=<0|1> entities=<E> converged=<0|1> iterations=<I>"
    harness.registry().registerCommand("param_samples.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const size_t entities = state->hasModel
                ? state->model->graph.entityCount() : 0u;

            messages.push_back("param_samples.describe"
                " has_model="  + std::string(state->hasModel ? "1" : "0") +
                " entities="   + std::to_string(entities) +
                " converged="  + std::string(state->lastConverged ? "1" : "0") +
                " iterations=" + std::to_string(state->lastIterations));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
