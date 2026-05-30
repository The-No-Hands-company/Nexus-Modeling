// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Script Expression DSL extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/ScriptExpressionExtension.h>
#include <nexus/script/Expression.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
exprGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

struct ExprState {
    std::optional<nexus::script::Expression> expr;
    std::string                              source;
    bool                                     hasExpr = false;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerScriptExpressionCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<ExprState>();

    // ── expr_dsl.compile ──────────────────────────────────────────────────────
    //
    // Compiles an expression from source text.
    //
    // Arguments:
    //   src=<expression>   the expression source (required)
    //
    // On success: "expr_dsl.compile ok vars=<N>"
    // On error:   "expr_dsl.compile fail: <diagnostic>" (one per error)
    //             Returns false.
    harness.registry().registerCommand("expr_dsl.compile",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto srcOpt = exprGetArg(cmd, "src");
            if (!srcOpt || srcOpt->empty()) {
                messages.push_back("expr_dsl.compile error: src required");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string src(*srcOpt);

            nexus::script::CompileResult result;
            auto compiled = nexus::script::Expression::compileWithDiagnostics(src, result);

            if (!result.ok || !compiled) {
                for (const auto& e : result.errors)
                    messages.push_back("expr_dsl.compile fail: " + e);
                if (messages.empty())
                    messages.push_back("expr_dsl.compile fail: unknown error");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const auto vars = compiled->variableNames();
            state->expr   = std::move(compiled);
            state->source = src;
            state->hasExpr = true;

            messages.push_back("expr_dsl.compile ok vars=" +
                std::to_string(vars.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── expr_dsl.evaluate ─────────────────────────────────────────────────────
    //
    // Evaluates the current compiled expression.  Variable bindings are passed
    // as args with the prefix "var_":  var_x=3.14  binds "x" → 3.14.
    //
    // On success: "expr_dsl.evaluate ok result=<R>"
    // On error:   "expr_dsl.evaluate error: ..." (no expr, or eval returned nullopt)
    harness.registry().registerCommand("expr_dsl.evaluate",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasExpr) {
                messages.push_back("expr_dsl.evaluate error: no compiled expression");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            // Collect var_* bindings
            std::unordered_map<std::string, double> vars;
            for (const auto& [k, v] : cmd.args) {
                if (k.size() > 4 && k.substr(0, 4) == "var_") {
                    const std::string varName = k.substr(4);
                    double val = 0.0;
                    std::from_chars(v.data(), v.data() + v.size(), val);
                    vars[varName] = val;
                }
            }

            const auto result = state->expr->evaluate(vars);
            if (!result) {
                messages.push_back("expr_dsl.evaluate error: evaluation failed"
                    " (missing variable or domain error)");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("expr_dsl.evaluate ok result=" +
                std::to_string(*result));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── expr_dsl.vars ─────────────────────────────────────────────────────────
    //
    // Reports the free variable names referenced by the current expression.
    // Returns false when no expression is compiled.
    //
    // On success: "expr_dsl.vars ok count=<N>" + "expr_dsl.vars name=<V>" per var
    harness.registry().registerCommand("expr_dsl.vars",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasExpr) {
                messages.push_back("expr_dsl.vars error: no compiled expression");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const auto vars = state->expr->variableNames();
            messages.push_back("expr_dsl.vars ok count=" +
                std::to_string(vars.size()));
            for (const auto& v : vars)
                messages.push_back("expr_dsl.vars name=" + v);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── expr_dsl.describe ─────────────────────────────────────────────────────
    //
    // Reports whether an expression is compiled and its source text.
    // Always returns true.
    //
    // "expr_dsl.describe compiled=<0|1> src=<S>"
    harness.registry().registerCommand("expr_dsl.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("expr_dsl.describe"
                " compiled=" + std::string(state->hasExpr ? "1" : "0") +
                " src="      + (state->hasExpr ? state->source : "(none)"));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── expr_dsl.clear ────────────────────────────────────────────────────────
    //
    // Drops the currently compiled expression.  Always returns true.
    //
    // On success: "expr_dsl.clear ok"
    harness.registry().registerCommand("expr_dsl.clear",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->expr.reset();
            state->source.clear();
            state->hasExpr = false;

            messages.push_back("expr_dsl.clear ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
