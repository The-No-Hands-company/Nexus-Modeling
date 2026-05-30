// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — NodeScene extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/NodeSceneExtension.h>
#include <nexus/eval/EvalGraph.h>
#include <nexus/scene/NodeScene.h>
#include <nexus/scene/NodeSceneSerializer.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
sceneGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = sceneGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
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

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerNodeSceneCommands(ScriptBatchHarness& harness)
{
    // ── scene.add_node ────────────────────────────────────────────────────────
    //
    // Adds a named, typed node to ctx.nodeScene.
    //
    // Arguments:
    //   name=<str>   node name (must be unique within the scene; default "node")
    //   kind=geometry|animation|transform|merge|proxy_geometry
    //               |reconstruction|expression|constant  (default constant)
    //
    // On success: "scene.add_node ok id=<N> kind=<K> name=<N>"
    harness.registry().registerCommand("scene.add_node",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            std::string name = "node";
            if (const auto v = sceneGetArg(cmd, "name"))
                name = std::string(*v);

            nexus::NodeKind kind = nexus::NodeKind::Constant;
            if (const auto v = sceneGetArg(cmd, "kind"))
                kind = parseKind(*v);

            const nexus::SceneNodeId id = ctx.nodeScene.addNode(name, kind);
            if (id == nexus::kInvalidSceneNodeId) {
                messages.push_back("scene.add_node error: node name already exists or invalid: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }
            ctx.hasNodeScene = true;

            messages.push_back("scene.add_node ok"
                " id="   + std::to_string(id) +
                " kind=" + std::string(kindStr(kind)) +
                " name=" + name);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene.connect ─────────────────────────────────────────────────────────
    //
    // Adds a dependency edge from src to dst in the scene.
    //
    // Arguments:
    //   src=<N>   source node id
    //   dst=<N>   destination node id
    //
    // On success: "scene.connect ok src=<N> dst=<N>"
    harness.registry().registerCommand("scene.connect",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const nexus::SceneNodeId src = uintArg(cmd, "src", nexus::kInvalidSceneNodeId);
            const nexus::SceneNodeId dst = uintArg(cmd, "dst", nexus::kInvalidSceneNodeId);

            if (src == nexus::kInvalidSceneNodeId || dst == nexus::kInvalidSceneNodeId) {
                messages.push_back("scene.connect error: src and dst must be non-zero ids");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.nodeScene.hasNode(src)) {
                messages.push_back("scene.connect error: src id=" + std::to_string(src) + " not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.nodeScene.hasNode(dst)) {
                messages.push_back("scene.connect error: dst id=" + std::to_string(dst) + " not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.nodeScene.connect(src, dst)) {
                messages.push_back("scene.connect error: edge already exists or self-loop src="
                    + std::to_string(src) + " dst=" + std::to_string(dst));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("scene.connect ok"
                " src=" + std::to_string(src) +
                " dst=" + std::to_string(dst));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene.disconnect ──────────────────────────────────────────────────────
    //
    // Removes a dependency edge from src to dst.
    //
    // Arguments:
    //   src=<N>   source node id
    //   dst=<N>   destination node id
    //
    // On success: "scene.disconnect ok src=<N> dst=<N>"
    harness.registry().registerCommand("scene.disconnect",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const nexus::SceneNodeId src = uintArg(cmd, "src", nexus::kInvalidSceneNodeId);
            const nexus::SceneNodeId dst = uintArg(cmd, "dst", nexus::kInvalidSceneNodeId);

            if (!ctx.nodeScene.disconnect(src, dst)) {
                messages.push_back("scene.disconnect error: edge not found src="
                    + std::to_string(src) + " dst=" + std::to_string(dst));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("scene.disconnect ok"
                " src=" + std::to_string(src) +
                " dst=" + std::to_string(dst));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene.set_parent ──────────────────────────────────────────────────────
    //
    // Sets child's scene-hierarchy parent.
    //
    // Arguments:
    //   child=<N>    child node id
    //   parent=<N>   parent node id
    //
    // On success: "scene.set_parent ok child=<C> parent=<P>"
    harness.registry().registerCommand("scene.set_parent",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const nexus::SceneNodeId child  = uintArg(cmd, "child",  nexus::kInvalidSceneNodeId);
            const nexus::SceneNodeId parent = uintArg(cmd, "parent", nexus::kInvalidSceneNodeId);

            if (!ctx.nodeScene.hasNode(child)) {
                messages.push_back("scene.set_parent error: child id=" + std::to_string(child) + " not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.nodeScene.hasNode(parent)) {
                messages.push_back("scene.set_parent error: parent id=" + std::to_string(parent) + " not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.nodeScene.setParent(child, parent)) {
                messages.push_back("scene.set_parent error: cycle or invalid assignment child="
                    + std::to_string(child) + " parent=" + std::to_string(parent));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("scene.set_parent ok"
                " child="  + std::to_string(child) +
                " parent=" + std::to_string(parent));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene.clear_parent ────────────────────────────────────────────────────
    //
    // Removes child from its parent in the scene hierarchy.
    //
    // Arguments:
    //   child=<N>   node id
    //
    // On success: "scene.clear_parent ok child=<N>"
    harness.registry().registerCommand("scene.clear_parent",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const nexus::SceneNodeId child = uintArg(cmd, "child", nexus::kInvalidSceneNodeId);

            if (!ctx.nodeScene.hasNode(child)) {
                messages.push_back("scene.clear_parent error: node id=" + std::to_string(child) + " not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            ctx.nodeScene.clearParent(child);
            messages.push_back("scene.clear_parent ok child=" + std::to_string(child));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene.evaluate ────────────────────────────────────────────────────────
    //
    // Evaluates the underlying EvalGraph in topological order.
    // Informational — always returns true (cycle is a warn, not a hard error).
    //
    // On success: "scene.evaluate ok nodes=<N> dirty=<D> order=<O>"
    // On cycle:   "scene.evaluate warn cycle_detected nodes=<N>"
    harness.registry().registerCommand("scene.evaluate",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            const nexus::EvalReport report = ctx.nodeScene.evaluate();

            if (report.hasCycle) {
                messages.push_back("scene.evaluate warn cycle_detected"
                    " nodes=" + std::to_string(ctx.nodeScene.nodeCount()));
                for (const auto& m : report.messages)
                    messages.push_back("scene.evaluate diagnostic: " + m);
                std::sort(messages.begin(), messages.end());
                return true;
            }

            messages.push_back("scene.evaluate ok"
                " nodes=" + std::to_string(ctx.nodeScene.nodeCount()) +
                " dirty=" + std::to_string(report.dirtyNodes.size()) +
                " order=" + std::to_string(report.evaluationOrder.size()));
            for (const auto& m : report.messages)
                messages.push_back("scene.evaluate diagnostic: " + m);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene.describe ────────────────────────────────────────────────────────
    //
    // Reports scene structure: node count, node names/kinds/parent for up to
    // 12 nodes, and their scene paths.  Always returns true (informational).
    harness.registry().registerCommand("scene.describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            const std::size_t nNodes = ctx.nodeScene.nodeCount();

            messages.push_back("scene.describe nodes=" + std::to_string(nNodes));

            const auto allIds = ctx.nodeScene.allNodeIds();

            const std::size_t limit = std::min(allIds.size(), std::size_t{12});
            for (std::size_t i = 0; i < limit; ++i) {
                const nexus::SceneNodeId id = allIds[i];
                const nexus::SceneNodeId par = ctx.nodeScene.parent(id);
                const std::string path = ctx.nodeScene.nodePath(id);
                messages.push_back("scene.describe node"
                    " id="     + std::to_string(id) +
                    " kind="   + std::string(kindStr(ctx.nodeScene.nodeKind(id))) +
                    " name="   + ctx.nodeScene.nodeName(id) +
                    " parent=" + (par == nexus::kInvalidSceneNodeId
                                      ? "none"
                                      : std::to_string(par)) +
                    " path="   + path);
            }

            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene.serialize ───────────────────────────────────────────────────────
    //
    // Serializes ctx.nodeScene to text and stores it in an internal buffer.
    //
    // On success: "scene.serialize ok nodes=<N> edges=<E>"
    // On error:   "scene.serialize fail: <diagnostic>"
    auto serialized = std::make_shared<std::string>();
    auto hasSerialized = std::make_shared<bool>(false);

    harness.registry().registerCommand("scene.serialize",
        [serialized, hasSerialized](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            nexus::NodeSceneSerializationReport report;
            const std::string data =
                nexus::NodeSceneSerializer::serialize(ctx.nodeScene, report);

            if (!report.ok) {
                for (const auto& e : report.errors)
                    messages.push_back("scene.serialize fail: " + e);
                if (messages.empty())
                    messages.push_back("scene.serialize fail: unknown error");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            *serialized    = data;
            *hasSerialized = true;
            messages.push_back("scene.serialize ok"
                " nodes=" + std::to_string(report.nodeCount) +
                " edges=" + std::to_string(report.edgeCount));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene.deserialize ─────────────────────────────────────────────────────
    //
    // Deserializes the last serialized text back into ctx.nodeScene.
    // Returns false when nothing has been serialized yet.
    //
    // On success: "scene.deserialize ok nodes=<N> edges=<E>"
    // On error:   "scene.deserialize error: ..."
    harness.registry().registerCommand("scene.deserialize",
        [serialized, hasSerialized](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!*hasSerialized) {
                messages.push_back("scene.deserialize error: nothing serialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::NodeScene out;
            const auto report =
                nexus::NodeSceneSerializer::deserialize(*serialized, out);

            if (!report.ok) {
                for (const auto& e : report.errors)
                    messages.push_back("scene.deserialize error: " + e);
                if (messages.empty())
                    messages.push_back("scene.deserialize error: parse failure");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.nodeScene = std::move(out);
            messages.push_back("scene.deserialize ok"
                " nodes=" + std::to_string(report.nodeCount) +
                " edges=" + std::to_string(report.edgeCount));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
