// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — EvalGraph extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/EvalExtension.h>
#include <nexus/eval/EvalGraph.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
evalGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = evalGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static nexus::NodeKind parseNodeKind(std::string_view s)
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

[[nodiscard]] static const char* kindName(nexus::NodeKind k)
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

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerEvalCommands(ScriptBatchHarness& harness)
{
    // ── eval.add_node ─────────────────────────────────────────────────────────
    //
    // Adds a typed node to ctx.evalGraph.
    //
    // Arguments:
    //   kind=geometry|animation|transform|merge|proxy_geometry|reconstruction
    //        |expression|constant  (default constant)
    //   name=<str>                 (default "node")
    //
    // On success: "eval.add_node ok id=<N> kind=<K> name=<N>"
    harness.registry().registerCommand("eval.add_node",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            nexus::NodeKind kind = nexus::NodeKind::Constant;
            if (const auto v = evalGetArg(cmd, "kind"))
                kind = parseNodeKind(*v);

            std::string name = "node";
            if (const auto v = evalGetArg(cmd, "name"))
                name = std::string(*v);

            const nexus::NodeId id = ctx.evalGraph.addNode(kind, name);
            if (id == nexus::kInvalidNodeId) {
                messages.push_back("eval.add_node error: addNode returned invalid id");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            ctx.hasEvalGraph = true;

            messages.push_back("eval.add_node ok"
                " id="   + std::to_string(id) +
                " kind=" + std::string(kindName(kind)) +
                " name=" + name);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── eval.connect ──────────────────────────────────────────────────────────
    //
    // Adds a directed edge from src to dst in ctx.evalGraph.
    //
    // Arguments:
    //   src=<N>   source node id
    //   dst=<N>   destination node id
    //
    // On success: "eval.connect ok src=<N> dst=<N>"
    harness.registry().registerCommand("eval.connect",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const nexus::NodeId src = uintArg(cmd, "src", nexus::kInvalidNodeId);
            const nexus::NodeId dst = uintArg(cmd, "dst", nexus::kInvalidNodeId);

            if (src == nexus::kInvalidNodeId || dst == nexus::kInvalidNodeId) {
                messages.push_back("eval.connect error: src and dst must be non-zero node ids");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.evalGraph.hasNode(src)) {
                messages.push_back("eval.connect error: src id=" + std::to_string(src) + " not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.evalGraph.hasNode(dst)) {
                messages.push_back("eval.connect error: dst id=" + std::to_string(dst) + " not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.evalGraph.connect(src, dst)) {
                messages.push_back("eval.connect error: edge already exists or self-loop src="
                    + std::to_string(src) + " dst=" + std::to_string(dst));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("eval.connect ok"
                " src=" + std::to_string(src) +
                " dst=" + std::to_string(dst));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── eval.disconnect ───────────────────────────────────────────────────────
    //
    // Removes a directed edge from src to dst.
    //
    // Arguments:
    //   src=<N>   source node id
    //   dst=<N>   destination node id
    //
    // On success: "eval.disconnect ok src=<N> dst=<N>"
    harness.registry().registerCommand("eval.disconnect",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const nexus::NodeId src = uintArg(cmd, "src", nexus::kInvalidNodeId);
            const nexus::NodeId dst = uintArg(cmd, "dst", nexus::kInvalidNodeId);

            if (!ctx.evalGraph.disconnect(src, dst)) {
                messages.push_back("eval.disconnect error: edge not found src="
                    + std::to_string(src) + " dst=" + std::to_string(dst));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("eval.disconnect ok"
                " src=" + std::to_string(src) +
                " dst=" + std::to_string(dst));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── eval.mark_dirty ───────────────────────────────────────────────────────
    //
    // Marks a node and all its downstream dependents dirty.
    //
    // Arguments:
    //   id=<N>   node id to mark dirty
    //
    // On success: "eval.mark_dirty ok id=<N>"
    harness.registry().registerCommand("eval.mark_dirty",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const nexus::NodeId id = uintArg(cmd, "id", nexus::kInvalidNodeId);
            if (!ctx.evalGraph.hasNode(id)) {
                messages.push_back("eval.mark_dirty error: node id=" + std::to_string(id) + " not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            ctx.evalGraph.markDirty(id);
            messages.push_back("eval.mark_dirty ok id=" + std::to_string(id));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── eval.evaluate ─────────────────────────────────────────────────────────
    //
    // Evaluates the graph in topological order. Dirty nodes are re-evaluated.
    // Always informational (cycle errors are reported but command returns true
    // so pipelines can inspect messages and continue).
    //
    // Arguments:  (none)
    //
    // On success:    "eval.evaluate ok nodes=<N> dirty=<D> order=<O>"
    // On cycle:      "eval.evaluate warn cycle_detected nodes=<N>"
    harness.registry().registerCommand("eval.evaluate",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            const nexus::EvalReport report = ctx.evalGraph.evaluate();

            if (report.hasCycle) {
                messages.push_back("eval.evaluate warn cycle_detected"
                    " nodes=" + std::to_string(ctx.evalGraph.nodeCount()));
                for (const auto& m : report.messages)
                    messages.push_back("eval.evaluate diagnostic: " + m);
                std::sort(messages.begin(), messages.end());
                return true; // informational
            }

            messages.push_back("eval.evaluate ok"
                " nodes="  + std::to_string(ctx.evalGraph.nodeCount()) +
                " dirty="  + std::to_string(report.dirtyNodes.size()) +
                " order="  + std::to_string(report.evaluationOrder.size()));
            for (const auto& m : report.messages)
                messages.push_back("eval.evaluate diagnostic: " + m);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── eval.describe ─────────────────────────────────────────────────────────
    //
    // Reports current graph structure: node count, edge count, and per-node
    // kind+name for up to 12 nodes.  Always returns true (informational).
    harness.registry().registerCommand("eval.describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            const auto ids = ctx.evalGraph.allNodeIds();

            // Count edges
            std::size_t edgeCount = 0;
            for (const auto id : ids) {
                const auto connected = ctx.evalGraph.allNodeIds();
                for (const auto other : connected)
                    if (ctx.evalGraph.isConnected(id, other)) ++edgeCount;
            }

            messages.push_back("eval.describe"
                " nodes=" + std::to_string(ids.size()) +
                " edges=" + std::to_string(edgeCount) +
                " has_cycle=" + (ctx.evalGraph.hasCycle() ? "true" : "false"));

            const std::size_t limit = std::min(ids.size(), std::size_t{12});
            for (std::size_t i = 0; i < limit; ++i) {
                const nexus::NodeId id = ids[i];
                messages.push_back("eval.describe node"
                    " id="   + std::to_string(id) +
                    " kind=" + std::string(kindName(ctx.evalGraph.nodeKind(id))) +
                    " name=" + ctx.evalGraph.nodeName(id) +
                    " dirty=" + (ctx.evalGraph.isDirty(id) ? "true" : "false"));
            }

            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
