// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Subgraph extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/SubgraphExtension.h>
#include <nexus/eval/EvalGraph.h>
#include <nexus/eval_ext/Subgraph.h>
#include <nexus/eval_ext/SubgraphRegistry.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
sgGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static nexus::NodeKind parseKind(std::string_view s)
{
    if (s == "geometry")       return nexus::NodeKind::Geometry;
    if (s == "animation")      return nexus::NodeKind::Animation;
    if (s == "transform")      return nexus::NodeKind::Transform;
    if (s == "merge")          return nexus::NodeKind::Merge;
    if (s == "proxy_geometry") return nexus::NodeKind::ProxyGeometry;
    if (s == "reconstruction") return nexus::NodeKind::Reconstruction;
    if (s == "expression")     return nexus::NodeKind::Expression;
    return nexus::NodeKind::Constant;
}

[[nodiscard]] static const char* kindStr(nexus::NodeKind k)
{
    switch (k) {
        case nexus::NodeKind::Geometry:       return "geometry";
        case nexus::NodeKind::Animation:      return "animation";
        case nexus::NodeKind::Transform:      return "transform";
        case nexus::NodeKind::Merge:          return "merge";
        case nexus::NodeKind::ProxyGeometry:  return "proxy_geometry";
        case nexus::NodeKind::Reconstruction: return "reconstruction";
        case nexus::NodeKind::Expression:     return "expression";
        case nexus::NodeKind::Constant:       return "constant";
    }
    return "constant";
}

struct SubgraphState {
    nexus::eval_ext::SubgraphTemplate   tmpl;
    bool                                hasTmpl = false;
    std::string                         tmplName;
    nexus::eval_ext::SubgraphRegistry   registry;
    // prefix → instance (for post-instantiation port queries)
    std::unordered_map<std::string, nexus::eval_ext::SubgraphInstance> instances;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerSubgraphCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<SubgraphState>();

    // ── subgraph.template_new ─────────────────────────────────────────────────
    //
    // Starts a fresh SubgraphTemplate for editing.  Any in-progress template is
    // discarded.
    //
    // Arguments:
    //   name=<str>   template name used later for registry registration (default "tmpl")
    //
    // On success: "subgraph.template_new ok name=<N>"
    harness.registry().registerCommand("subgraph.template_new",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            std::string name = "tmpl";
            if (const auto v = sgGetArg(cmd, "name"))
                name = std::string(*v);

            state->tmpl     = nexus::eval_ext::SubgraphTemplate{};
            state->hasTmpl  = true;
            state->tmplName = name;

            messages.push_back("subgraph.template_new ok name=" + name);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.add_node ─────────────────────────────────────────────────────
    //
    // Adds a node to the current template.
    //
    // Arguments:
    //   name=<str>   node name (default "node")
    //   kind=<str>   node kind (default "constant")
    //
    // On success: "subgraph.add_node ok local_id=<I> name=<N> kind=<K>"
    // On error:   "subgraph.add_node error: no template"
    harness.registry().registerCommand("subgraph.add_node",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasTmpl) {
                messages.push_back("subgraph.add_node error: no template");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            std::string name = "node";
            if (const auto v = sgGetArg(cmd, "name"))
                name = std::string(*v);

            nexus::NodeKind kind = nexus::NodeKind::Constant;
            if (const auto v = sgGetArg(cmd, "kind"))
                kind = parseKind(*v);

            const nexus::eval_ext::LocalNodeId id =
                state->tmpl.addNode(kind, name);

            messages.push_back("subgraph.add_node ok"
                " local_id=" + std::to_string(id) +
                " name="     + name +
                " kind="     + std::string(kindStr(kind)));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.connect ──────────────────────────────────────────────────────
    //
    // Connects two local nodes in the current template.
    //
    // Arguments:
    //   src=<N>   source local node id
    //   dst=<N>   destination local node id
    //
    // On success: "subgraph.connect ok src=<S> dst=<D>"
    // On error:   "subgraph.connect error: ..."
    harness.registry().registerCommand("subgraph.connect",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasTmpl) {
                messages.push_back("subgraph.connect error: no template");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::eval_ext::LocalNodeId src = 0, dst = 0;
            if (const auto v = sgGetArg(cmd, "src"))
                std::from_chars(v->data(), v->data() + v->size(), src);
            if (const auto v = sgGetArg(cmd, "dst"))
                std::from_chars(v->data(), v->data() + v->size(), dst);

            if (!state->tmpl.connect(src, dst)) {
                messages.push_back("subgraph.connect error: src=" +
                    std::to_string(src) + " dst=" + std::to_string(dst));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("subgraph.connect ok"
                " src=" + std::to_string(src) +
                " dst=" + std::to_string(dst));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.declare_input ────────────────────────────────────────────────
    //
    // Declares a named input port on the current template.
    //
    // Arguments:
    //   port=<str>   port name (required)
    //   node=<N>     local node id that acts as the entry point
    //
    // On success: "subgraph.declare_input ok port=<P> node=<N>"
    // On error:   "subgraph.declare_input error: ..."
    harness.registry().registerCommand("subgraph.declare_input",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasTmpl) {
                messages.push_back("subgraph.declare_input error: no template");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            std::string port;
            if (const auto v = sgGetArg(cmd, "port"))
                port = std::string(*v);
            if (port.empty()) {
                messages.push_back("subgraph.declare_input error: port name is required");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::eval_ext::LocalNodeId nodeId = 0;
            if (const auto v = sgGetArg(cmd, "node"))
                std::from_chars(v->data(), v->data() + v->size(), nodeId);

            if (!state->tmpl.declareInputPort(port, nodeId)) {
                messages.push_back("subgraph.declare_input error: failed for port=" + port);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("subgraph.declare_input ok"
                " port=" + port +
                " node=" + std::to_string(nodeId));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.declare_output ───────────────────────────────────────────────
    //
    // Declares a named output port on the current template.
    //
    // Arguments:
    //   port=<str>   port name (required)
    //   node=<N>     local node id that provides the output
    //
    // On success: "subgraph.declare_output ok port=<P> node=<N>"
    // On error:   "subgraph.declare_output error: ..."
    harness.registry().registerCommand("subgraph.declare_output",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasTmpl) {
                messages.push_back("subgraph.declare_output error: no template");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            std::string port;
            if (const auto v = sgGetArg(cmd, "port"))
                port = std::string(*v);
            if (port.empty()) {
                messages.push_back("subgraph.declare_output error: port name is required");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::eval_ext::LocalNodeId nodeId = 0;
            if (const auto v = sgGetArg(cmd, "node"))
                std::from_chars(v->data(), v->data() + v->size(), nodeId);

            if (!state->tmpl.declareOutputPort(port, nodeId)) {
                messages.push_back("subgraph.declare_output error: failed for port=" + port);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("subgraph.declare_output ok"
                " port=" + port +
                " node=" + std::to_string(nodeId));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.validate ─────────────────────────────────────────────────────
    //
    // Validates the current template (cycle detection, port integrity).
    // Always returns true (informational — reports issues as extra messages).
    //
    // On success: "subgraph.validate ok nodes=<N> edges=<E> inputs=<I> outputs=<O>"
    // On issues:  "subgraph.validate warn issues=<K>" + per-issue lines
    harness.registry().registerCommand("subgraph.validate",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasTmpl) {
                messages.push_back("subgraph.validate warn no_template");
                std::sort(messages.begin(), messages.end());
                return true;
            }

            const nexus::eval_ext::SubgraphValidation v = state->tmpl.validate();

            if (!v.ok || !v.issues.empty()) {
                messages.push_back("subgraph.validate warn"
                    " issues=" + std::to_string(v.issues.size()) +
                    (v.hasCycle ? " has_cycle=1" : ""));
                for (const auto& iss : v.issues)
                    messages.push_back("subgraph.validate issue: " + iss);
            } else {
                messages.push_back("subgraph.validate ok"
                    " nodes="   + std::to_string(state->tmpl.nodeCount()) +
                    " edges="   + std::to_string(state->tmpl.edgeCount()) +
                    " inputs="  + std::to_string(state->tmpl.inputPortNames().size()) +
                    " outputs=" + std::to_string(state->tmpl.outputPortNames().size()));
            }
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.instantiate ──────────────────────────────────────────────────
    //
    // Instantiates the current template into ctx.evalGraph with a name prefix.
    //
    // Arguments:
    //   prefix=<str>   prefix for cloned node names (default "inst")
    //
    // On success: "subgraph.instantiate ok prefix=<P> host_nodes=<N>"
    // On error:   "subgraph.instantiate error: ..."
    harness.registry().registerCommand("subgraph.instantiate",
        [state](ScriptContext& ctx, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasTmpl) {
                messages.push_back("subgraph.instantiate error: no template");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            std::string prefix = "inst";
            if (const auto v = sgGetArg(cmd, "prefix"))
                prefix = std::string(*v);

            auto inst = nexus::eval_ext::instantiateSubgraph(
                ctx.evalGraph, state->tmpl, prefix);

            if (!inst.has_value()) {
                messages.push_back("subgraph.instantiate error: template failed validation or host error prefix=" + prefix);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const size_t nHost = inst->hostNodes.size();
            state->instances[prefix] = std::move(*inst);
            ctx.hasEvalGraph = true;

            messages.push_back("subgraph.instantiate ok"
                " prefix="     + prefix +
                " host_nodes=" + std::to_string(nHost));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.describe ─────────────────────────────────────────────────────
    //
    // Reports current template state and registry size. Always returns true.
    harness.registry().registerCommand("subgraph.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (state->hasTmpl) {
                messages.push_back("subgraph.describe template"
                    " name="    + state->tmplName +
                    " nodes="   + std::to_string(state->tmpl.nodeCount()) +
                    " edges="   + std::to_string(state->tmpl.edgeCount()) +
                    " inputs="  + std::to_string(state->tmpl.inputPortNames().size()) +
                    " outputs=" + std::to_string(state->tmpl.outputPortNames().size()));
            } else {
                messages.push_back("subgraph.describe template none");
            }
            messages.push_back("subgraph.describe registry size=" +
                std::to_string(state->registry.size()) +
                " instances=" + std::to_string(state->instances.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.registry_add ─────────────────────────────────────────────────
    //
    // Registers the current template into the local registry under its name.
    //
    // On success: "subgraph.registry_add ok name=<N> registry_size=<S>"
    // On error:   "subgraph.registry_add error: ..."
    harness.registry().registerCommand("subgraph.registry_add",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasTmpl) {
                messages.push_back("subgraph.registry_add error: no template");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            // Validate before registering.
            const auto vr = state->tmpl.validate();
            if (!vr.ok) {
                messages.push_back("subgraph.registry_add error: template invalid name=" +
                    state->tmplName);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            if (!state->registry.add(state->tmplName, state->tmpl)) {
                messages.push_back("subgraph.registry_add error: name already registered or invalid: " +
                    state->tmplName);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("subgraph.registry_add ok"
                " name="          + state->tmplName +
                " registry_size=" + std::to_string(state->registry.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.registry_list ────────────────────────────────────────────────
    //
    // Lists all registered template names. Always returns true.
    //
    // On success: "subgraph.registry_list ok count=<N>" + name lines
    harness.registry().registerCommand("subgraph.registry_list",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto names = state->registry.names();
            messages.push_back("subgraph.registry_list ok count=" +
                std::to_string(names.size()));
            for (const auto& n : names)
                messages.push_back("subgraph.registry_list name=" + n);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph.registry_remove ──────────────────────────────────────────────
    //
    // Removes a template from the registry by name.
    //
    // Arguments:
    //   name=<str>   template name to remove (required)
    //
    // On success: "subgraph.registry_remove ok name=<N> registry_size=<S>"
    // On error:   "subgraph.registry_remove error: ..."
    harness.registry().registerCommand("subgraph.registry_remove",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            std::string name;
            if (const auto v = sgGetArg(cmd, "name"))
                name = std::string(*v);
            if (name.empty()) {
                messages.push_back("subgraph.registry_remove error: name is required");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            if (!state->registry.remove(name)) {
                messages.push_back("subgraph.registry_remove error: not found: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("subgraph.registry_remove ok"
                " name="          + name +
                " registry_size=" + std::to_string(state->registry.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
