// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — ExpressionNode extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/ExpressionNodeExtension.h>
#include <nexus/eval/EvalGraph.h>
#include <nexus/eval/ExpressionNode.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
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

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = exprGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

// Looks up a node id by name in the evalNodesByName map.
[[nodiscard]] static nexus::NodeId findNode(const ScriptContext& ctx,
                                             const std::string& name,
                                             bool& found)
{
    auto it = ctx.evalNodesByName.find(name);
    if (it == ctx.evalNodesByName.end()) { found = false; return {}; }
    found = true;
    return it->second;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerExpressionNodeCommands(ScriptBatchHarness& harness)
{
    // ── expr.add_constant ─────────────────────────────────────────────────────
    //
    // Adds a ScalarF32 constant node to ctx.evalGraph and registers its name.
    // Reuses EvalExtension's evalNodesByName map for cross-command lookup.
    //
    // Arguments:
    //   name=<str>    node name (must be unique; default "const")
    //   value=<f>     initial float payload (default 0.0)
    //
    // On success: "expr.add_constant ok id=<I> name=<N> value=<V>"
    // On error:   "expr.add_constant error: ..."
    harness.registry().registerCommand("expr.add_constant",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            std::string name = "const";
            if (const auto v = exprGetArg(cmd, "name"))
                name = std::string(*v);

            if (ctx.evalNodesByName.count(name)) {
                messages.push_back("expr.add_constant error: name already exists: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const float value = floatArg(cmd, "value", 0.f);

            const nexus::NodeId id =
                ctx.evalGraph.addNode(nexus::NodeKind::Constant, name);

            nexus::NodePayload payload;
            payload.value = value;
            [[maybe_unused]] bool payOk =
                ctx.evalGraph.setNodeOutputPayload(id, std::move(payload));

            ctx.evalNodesByName[name] = id;
            ctx.hasEvalGraph = true;

            messages.push_back("expr.add_constant ok"
                " id="    + std::to_string(id) +
                " name="  + name +
                " value=" + std::to_string(value));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── expr.add_adapter ──────────────────────────────────────────────────────
    //
    // Compiles an expression and attaches it as an ExpressionNodeAdapter to
    // ctx.evalGraph.  Each binding maps a free variable name to an existing
    // constant node name.
    //
    // Arguments:
    //   name=<str>        adapter node name (must be unique; default "expr")
    //   source=<str>      expression source (default "0")
    //   bind_<var>=<node> one or more variable → node-name bindings
    //                     e.g.  bind_x=myConst  bind_y=other
    //
    // On success: "expr.add_adapter ok id=<I> name=<N> bindings=<B>"
    // On error:   "expr.add_adapter error: ..."
    harness.registry().registerCommand("expr.add_adapter",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            std::string name = "expr";
            if (const auto v = exprGetArg(cmd, "name"))
                name = std::string(*v);

            if (ctx.evalNodesByName.count(name)) {
                messages.push_back("expr.add_adapter error: name already exists: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            std::string source = "0";
            if (const auto v = exprGetArg(cmd, "source"))
                source = std::string(*v);

            // Collect bind_<var>=<nodeName> args.
            std::vector<nexus::eval::ExpressionBinding> bindings;
            for (const auto& [k, v] : cmd.args) {
                if (k.size() > 5 && k.substr(0, 5) == "bind_") {
                    const std::string varName  = k.substr(5);
                    const std::string nodeName = v;
                    bool found = false;
                    const nexus::NodeId srcId = findNode(ctx, nodeName, found);
                    if (!found) {
                        messages.push_back("expr.add_adapter error: source node not found: " + nodeName);
                        std::sort(messages.begin(), messages.end());
                        return false;
                    }
                    bindings.push_back({varName, srcId});
                }
            }

            auto adapter = nexus::eval::ExpressionNodeAdapter::create(
                ctx.evalGraph, name, source, std::move(bindings));

            if (!adapter.has_value()) {
                messages.push_back("expr.add_adapter error: compile failed or invalid binding for source: " + source);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const nexus::NodeId id = adapter->nodeId();
            ctx.evalNodesByName[name] = id;
            ctx.exprAdapterNames.push_back(name);
            ctx.exprAdapters.push_back(std::move(*adapter));
            ctx.hasEvalGraph = true;

            messages.push_back("expr.add_adapter ok"
                " id="       + std::to_string(id) +
                " name="     + name +
                " bindings=" + std::to_string(ctx.exprAdapters.back().bindings().size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── expr.evaluate ─────────────────────────────────────────────────────────
    //
    // Evaluates ctx.evalGraph in topological order, running each
    // ExpressionNodeAdapter's compute callback for Expression-kind nodes.
    // Informational — always returns true.
    //
    // On success: "expr.evaluate ok nodes=<N> adapters=<A>"
    // On cycle:   "expr.evaluate warn cycle_detected nodes=<N>"
    harness.registry().registerCommand("expr.evaluate",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            // Wire up compute callbacks for all registered adapters.
            ctx.evalGraph.setComputeCallback(
                [&](nexus::NodeComputeContext& nctx) -> bool {
                    for (auto& adapter : ctx.exprAdapters) {
                        if (nctx.id == adapter.nodeId())
                            return adapter.compute(nctx);
                    }
                    return true; // passthrough for non-expression nodes
                });

            const nexus::EvalReport report = ctx.evalGraph.evaluate();

            if (report.hasCycle) {
                messages.push_back("expr.evaluate warn cycle_detected"
                    " nodes=" + std::to_string(ctx.evalGraph.nodeCount()));
                std::sort(messages.begin(), messages.end());
                return true;
            }

            messages.push_back("expr.evaluate ok"
                " nodes="    + std::to_string(ctx.evalGraph.nodeCount()) +
                " adapters=" + std::to_string(ctx.exprAdapters.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── expr.read ─────────────────────────────────────────────────────────────
    //
    // Reads the ScalarF32 output payload of a named node after evaluation.
    //
    // Arguments:
    //   name=<str>   node name to read (required)
    //
    // On success: "expr.read ok name=<N> value=<V>"
    // On error:   "expr.read error: ..."
    harness.registry().registerCommand("expr.read",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            std::string name;
            if (const auto v = exprGetArg(cmd, "name"))
                name = std::string(*v);

            if (name.empty()) {
                messages.push_back("expr.read error: name is required");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            bool found = false;
            const nexus::NodeId id = findNode(ctx, name, found);
            if (!found) {
                messages.push_back("expr.read error: node not found: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const nexus::NodePayload* payload =
                ctx.evalGraph.nodeOutputPayload(id);

            if (!payload || !payload->scalarF32()) {
                messages.push_back("expr.read error: no ScalarF32 payload for node: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const float v = *payload->scalarF32();
            messages.push_back("expr.read ok"
                " name="  + name +
                " value=" + std::to_string(v));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── expr.describe ─────────────────────────────────────────────────────────
    //
    // Reports graph-level counts and per-adapter name/bindings for up to 8.
    // Always returns true.
    harness.registry().registerCommand("expr.describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            const size_t nNodes    = ctx.evalGraph.nodeCount();
            const size_t nAdapters = ctx.exprAdapters.size();

            messages.push_back("expr.describe nodes=" + std::to_string(nNodes) +
                               " adapters=" + std::to_string(nAdapters));

            const size_t limit = std::min(nAdapters, size_t{8});
            for (size_t i = 0; i < limit; ++i) {
                messages.push_back("expr.describe adapter"
                    " index="    + std::to_string(i) +
                    " name="     + ctx.exprAdapterNames[i] +
                    " bindings=" + std::to_string(ctx.exprAdapters[i].bindings().size()));
            }

            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
