// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — SceneGraph and Camera extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/SceneGraphExtension.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/render/Camera.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
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

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = sgGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

struct SceneGraphState {
    nexus::render::SceneGraph graph;
    nexus::render::Camera     camera;
    // Track all created node names → NodeID for lookup
    std::unordered_map<std::string, nexus::render::Node::NodeID> nodeIds;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerSceneGraphCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<SceneGraphState>();

    // ── camera.set_perspective ────────────────────────────────────────────────
    //
    // Configures the camera as a perspective projection.
    //
    // Arguments:
    //   fov=<f>     vertical field of view in degrees  (default 60.0)
    //   aspect=<f>  width / height ratio               (default 1.777… = 16:9)
    //   near=<f>    near plane                         (default 0.1)
    //   far=<f>     far plane                          (default 10000.0)
    //
    // On success: "camera.set_perspective ok fov=<F> aspect=<A> near=<N> far=<R>"
    harness.registry().registerCommand("camera.set_perspective",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const float fov    = floatArg(cmd, "fov",    60.f);
            const float aspect = floatArg(cmd, "aspect", 16.f / 9.f);
            const float near_  = floatArg(cmd, "near",   0.1f);
            const float far_   = floatArg(cmd, "far",    10000.f);

            state->camera.setPerspective(fov, aspect, near_, far_);

            messages.push_back("camera.set_perspective ok"
                " fov="    + std::to_string(fov) +
                " aspect=" + std::to_string(aspect) +
                " near="   + std::to_string(near_) +
                " far="    + std::to_string(far_));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── camera.set_ortho ──────────────────────────────────────────────────────
    //
    // Configures the camera as an orthographic projection.
    //
    // Arguments:
    //   width=<f>   orthographic half-width  (default 10.0)
    //   height=<f>  orthographic half-height (default 10.0)
    //   near=<f>                             (default 0.1)
    //   far=<f>                              (default 1000.0)
    //
    // On success: "camera.set_ortho ok width=<W> height=<H> near=<N> far=<R>"
    harness.registry().registerCommand("camera.set_ortho",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const float w     = floatArg(cmd, "width",  10.f);
            const float h     = floatArg(cmd, "height", 10.f);
            const float near_ = floatArg(cmd, "near",   0.1f);
            const float far_  = floatArg(cmd, "far",    1000.f);

            state->camera.setOrthographic(w, h, near_, far_);

            messages.push_back("camera.set_ortho ok"
                " width="  + std::to_string(w) +
                " height=" + std::to_string(h) +
                " near="   + std::to_string(near_) +
                " far="    + std::to_string(far_));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── camera.look_at ────────────────────────────────────────────────────────
    //
    // Sets camera view matrix via eye / target / up vectors.
    //
    // Arguments:
    //   ex=<f> ey=<f> ez=<f>    eye position     (default 0 0 5)
    //   tx=<f> ty=<f> tz=<f>    target position  (default 0 0 0)
    //   ux=<f> uy=<f> uz=<f>    up vector        (default 0 1 0)
    //
    // On success: "camera.look_at ok eye=<ex,ey,ez> target=<tx,ty,tz>"
    harness.registry().registerCommand("camera.look_at",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const nexus::render::Vec3 eye    = {floatArg(cmd,"ex",0.f),
                                                floatArg(cmd,"ey",0.f),
                                                floatArg(cmd,"ez",5.f)};
            const nexus::render::Vec3 target = {floatArg(cmd,"tx",0.f),
                                                floatArg(cmd,"ty",0.f),
                                                floatArg(cmd,"tz",0.f)};
            const nexus::render::Vec3 up     = {floatArg(cmd,"ux",0.f),
                                                floatArg(cmd,"uy",1.f),
                                                floatArg(cmd,"uz",0.f)};
            state->camera.lookAt(eye, target, up);

            messages.push_back("camera.look_at ok"
                " eye="    + std::to_string(eye.x)    + "," + std::to_string(eye.y)    + "," + std::to_string(eye.z) +
                " target=" + std::to_string(target.x) + "," + std::to_string(target.y) + "," + std::to_string(target.z));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── camera.describe ───────────────────────────────────────────────────────
    //
    // Reports current camera position.  Always returns true.
    //
    // "camera.describe px=<X> py=<Y> pz=<Z>"
    harness.registry().registerCommand("camera.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto pos = state->camera.position();
            messages.push_back("camera.describe"
                " px=" + std::to_string(pos.x) +
                " py=" + std::to_string(pos.y) +
                " pz=" + std::to_string(pos.z));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sgraph.create_node ────────────────────────────────────────────────────
    //
    // Creates a named node in the render scene graph.
    //
    // Arguments:
    //   name=<str>    node name (required; must be unique)
    //   parent=<str>  parent node name; omit to attach to root (default "root")
    //
    // On success: "sgraph.create_node ok name=<N> id=<I>"
    // On error:   "sgraph.create_node error: ..."
    harness.registry().registerCommand("sgraph.create_node",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto nameOpt = sgGetArg(cmd, "name");
            if (!nameOpt || nameOpt->empty()) {
                messages.push_back("sgraph.create_node error: name required");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string name((*nameOpt));

            if (state->nodeIds.count(name)) {
                messages.push_back("sgraph.create_node error: node already exists: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::render::Node* parent = &state->graph.root();
            if (const auto parentName = sgGetArg(cmd, "parent")) {
                auto* p = state->graph.findNode(*parentName);
                if (!p) {
                    messages.push_back("sgraph.create_node error: parent not found: " + std::string(*parentName));
                    std::sort(messages.begin(), messages.end());
                    return false;
                }
                parent = p;
            }

            auto* node = state->graph.createNode(name, parent);
            state->nodeIds[name] = node->id();

            messages.push_back("sgraph.create_node ok"
                " name=" + name +
                " id="   + std::to_string(node->id()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sgraph.remove_node ────────────────────────────────────────────────────
    //
    // Removes a named node (and its subtree) from the scene graph.
    //
    // Arguments:
    //   name=<str>  node name (required)
    //
    // On success: "sgraph.remove_node ok name=<N>"
    // On error:   "sgraph.remove_node error: ..."
    harness.registry().registerCommand("sgraph.remove_node",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto nameOpt = sgGetArg(cmd, "name");
            if (!nameOpt || nameOpt->empty()) {
                messages.push_back("sgraph.remove_node error: name required");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string name(*nameOpt);

            auto it = state->nodeIds.find(name);
            if (it == state->nodeIds.end()) {
                messages.push_back("sgraph.remove_node error: node not found: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->graph.removeNode(it->second);
            state->nodeIds.erase(it);

            messages.push_back("sgraph.remove_node ok name=" + name);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sgraph.set_transform ──────────────────────────────────────────────────
    //
    // Sets the local translation of a named node.
    //
    // Arguments:
    //   name=<str>            node name (required)
    //   tx=<f> ty=<f> tz=<f>  translation (default 0 0 0)
    //
    // On success: "sgraph.set_transform ok name=<N>"
    // On error:   "sgraph.set_transform error: ..."
    harness.registry().registerCommand("sgraph.set_transform",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto nameOpt = sgGetArg(cmd, "name");
            if (!nameOpt || nameOpt->empty()) {
                messages.push_back("sgraph.set_transform error: name required");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string name(*nameOpt);

            auto* node = state->graph.findNode(std::string_view(name));
            if (!node) {
                messages.push_back("sgraph.set_transform error: node not found: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            auto& t = node->localTransform();
            t.translation = {floatArg(cmd,"tx",0.f),
                              floatArg(cmd,"ty",0.f),
                              floatArg(cmd,"tz",0.f)};
            node->markDirty();

            messages.push_back("sgraph.set_transform ok name=" + name);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sgraph.set_visible ────────────────────────────────────────────────────
    //
    // Sets the visibility flag on a named node.
    //
    // Arguments:
    //   name=<str>    node name (required)
    //   visible=0|1   0 = hidden, 1 = visible (default 1)
    //
    // On success: "sgraph.set_visible ok name=<N> visible=<V>"
    // On error:   "sgraph.set_visible error: ..."
    harness.registry().registerCommand("sgraph.set_visible",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto nameOpt = sgGetArg(cmd, "name");
            if (!nameOpt || nameOpt->empty()) {
                messages.push_back("sgraph.set_visible error: name required");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string name(*nameOpt);

            auto* node = state->graph.findNode(std::string_view(name));
            if (!node) {
                messages.push_back("sgraph.set_visible error: node not found: " + name);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const auto visStr = sgGetArg(cmd, "visible");
            node->visible = (!visStr || *visStr != "0");

            messages.push_back("sgraph.set_visible ok"
                " name="    + name +
                " visible=" + std::string(node->visible ? "1" : "0"));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sgraph.clear ──────────────────────────────────────────────────────────
    //
    // Removes all nodes from the scene graph (resets to empty root).
    //
    // On success: "sgraph.clear ok"
    harness.registry().registerCommand("sgraph.clear",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->graph.clear();
            state->nodeIds.clear();

            messages.push_back("sgraph.clear ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sgraph.describe ───────────────────────────────────────────────────────
    //
    // Reports node count (excluding the implicit root) and named nodes.
    // Always returns true.
    //
    // "sgraph.describe nodes=<N>"  followed by per-node lines.
    harness.registry().registerCommand("sgraph.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("sgraph.describe nodes=" +
                std::to_string(state->nodeIds.size()));

            for (const auto& [name, id] : state->nodeIds) {
                auto* node = state->graph.findNode(id);
                if (!node) continue;
                const auto& t = node->localTransform();
                messages.push_back("sgraph.describe node"
                    " name="    + name +
                    " id="      + std::to_string(id) +
                    " visible=" + std::string(node->visible ? "1" : "0") +
                    " tx="      + std::to_string(t.translation.x) +
                    " ty="      + std::to_string(t.translation.y) +
                    " tz="      + std::to_string(t.translation.z));
            }

            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
