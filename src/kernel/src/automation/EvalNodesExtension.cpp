// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — AnimationNodes, GeometryNodes, SubgraphSerialization
//  extension commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/EvalNodesExtension.h>
#include <nexus/eval/AnimationNodes.h>
#include <nexus/eval/GeometryNodes.h>
#include <nexus/eval_ext/SubgraphSerialization.h>
#include <nexus/eval_ext/Subgraph.h>
#include <nexus/animation/AnimationCore.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/RemeshOperation.h>
#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/InsetFacesOperation.h>

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
enGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = enGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

struct EvalNodesState {
    // AnimationNodes shared clip pair
    nexus::animation::AnimationClip clipA{1.0f, 30.0f};
    nexus::animation::AnimationClip clipB{1.0f, 30.0f};
    nexus::animation::AnimationClip clipOut;
    bool hasBlendResult = false;

    // SubgraphSerialization
    std::optional<nexus::eval_ext::SubgraphTemplate> subgraph;
    std::string serializedSubgraph;
    bool hasSerializedSubgraph = false;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerEvalNodesCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<EvalNodesState>();

    // ── anim_blend.compute ────────────────────────────────────────────────────
    //
    // Blends clipA and clipB using AnimationBlendNode::compute.
    //
    // Arguments:
    //   weight=<f>   blend weight 0..1 (default 0.5)
    //
    // On success: "anim_blend.compute ok weight=<W>"
    // On error:   "anim_blend.compute error: mismatched clips"
    harness.registry().registerCommand("anim_blend.compute",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const float weight = floatArg(cmd, "weight", 0.5f);

            nexus::animation::AnimationClip out;
            const bool ok = nexus::eval::AnimationBlendNode::compute(
                state->clipA, state->clipB, weight, out);

            if (!ok) {
                messages.push_back("anim_blend.compute error: mismatched clips");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->clipOut       = std::move(out);
            state->hasBlendResult = true;
            messages.push_back("anim_blend.compute ok"
                " weight=" + std::to_string(weight));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim_sample.compute ───────────────────────────────────────────────────
    //
    // Samples ctx.pose from ctx.clip (clipA) at a given time using
    // AnimationSampleNode::compute.
    //
    // Arguments:
    //   time=<f>   sample time in seconds (default 0.0)
    //
    // On success: "anim_sample.compute ok time=<T> bones=<B>"
    // On error:   "anim_sample.compute error: ..."
    harness.registry().registerCommand("anim_sample.compute",
        [state](ScriptContext& ctx, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const float t = floatArg(cmd, "time", 0.0f);

            nexus::animation::Pose outPose;
            const bool ok = nexus::eval::AnimationSampleNode::compute(
                state->clipA, ctx.skeleton, t, outPose);

            if (!ok) {
                messages.push_back("anim_sample.compute error: sample failed");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.pose = std::move(outPose);
            messages.push_back("anim_sample.compute ok"
                " time="  + std::to_string(t) +
                " bones=" + std::to_string(ctx.skeleton.boneCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim_retarget.compute ─────────────────────────────────────────────────
    //
    // Retargets ctx.pose from ctx.skeleton to ctx.skeletonBaseline using
    // AnimationRetargetNode::compute.
    //
    // On success: "anim_retarget.compute ok bones=<B>"
    // On error:   "anim_retarget.compute error: skeleton mismatch"
    harness.registry().registerCommand("anim_retarget.compute",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            nexus::animation::Pose outPose;
            const bool ok = nexus::eval::AnimationRetargetNode::compute(
                ctx.pose, ctx.skeleton, ctx.skeletonBaseline, outPose);

            if (!ok) {
                messages.push_back("anim_retarget.compute error: skeleton mismatch");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.poseBaseline = std::move(outPose);
            messages.push_back("anim_retarget.compute ok"
                " bones=" + std::to_string(ctx.skeletonBaseline.boneCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── geom_merge.compute ────────────────────────────────────────────────────
    //
    // Merges ctx.mesh with ctx.meshBaseline using GeometryMergeNode::compute.
    // Result is stored back in ctx.mesh.
    //
    // On success: "geom_merge.compute ok verts=<V> faces=<F>"
    // On error:   "geom_merge.compute error: ..."
    harness.registry().registerCommand("geom_merge.compute",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            nexus::geometry::Mesh out;
            const bool ok = nexus::eval::GeometryMergeNode::compute(
                ctx.mesh, ctx.meshBaseline, out);

            if (!ok) {
                messages.push_back("geom_merge.compute error: merge failed");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.mesh = std::move(out);
            messages.push_back("geom_merge.compute ok"
                " verts=" + std::to_string(ctx.mesh.attributes().vertexCount()) +
                " faces=" + std::to_string(ctx.mesh.topology().faceCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── geom_boolean.compute ──────────────────────────────────────────────────
    //
    // Applies GeometryBooleanNode::compute on ctx.mesh (A) and ctx.meshBaseline (B).
    //
    // Arguments:
    //   op=Union|Difference|Intersection   (default Union)
    //
    // On success: "geom_boolean.compute ok op=<O> verts=<V>"
    // On error:   "geom_boolean.compute error: operation failed"
    harness.registry().registerCommand("geom_boolean.compute",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const auto opStr = enGetArg(cmd, "op");
            nexus::geometry::BooleanOperationType op =
                nexus::geometry::BooleanOperationType::Union;
            std::string opName = "Union";
            if (opStr) {
                if (*opStr == "Difference")   { op = nexus::geometry::BooleanOperationType::Difference;   opName = "Difference"; }
                if (*opStr == "Intersection") { op = nexus::geometry::BooleanOperationType::Intersection; opName = "Intersection"; }
            }

            nexus::geometry::Mesh out;
            const bool ok = nexus::eval::GeometryBooleanNode::compute(
                ctx.mesh, ctx.meshBaseline, op, out);

            if (!ok) {
                messages.push_back("geom_boolean.compute error: operation failed");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.mesh = std::move(out);
            messages.push_back("geom_boolean.compute ok"
                " op="    + opName +
                " verts=" + std::to_string(ctx.mesh.attributes().vertexCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── geom_remesh.compute ───────────────────────────────────────────────────
    //
    // Applies GeometryRemeshNode::compute on ctx.mesh.
    //
    // Arguments:
    //   target_edge=<f>   target edge length (default 0.1)
    //
    // On success: "geom_remesh.compute ok verts=<V>"
    // On error:   "geom_remesh.compute error: ..."
    harness.registry().registerCommand("geom_remesh.compute",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            nexus::geometry::RemeshDesc desc{};
            desc.targetEdgeLength = floatArg(cmd, "target_edge", 0.1f);

            nexus::geometry::Mesh out;
            const bool ok = nexus::eval::GeometryRemeshNode::compute(
                ctx.mesh, desc, out);

            if (!ok) {
                messages.push_back("geom_remesh.compute error: remesh failed");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.mesh = std::move(out);
            messages.push_back("geom_remesh.compute ok"
                " verts=" + std::to_string(ctx.mesh.attributes().vertexCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph_serial.serialize ─────────────────────────────────────────────
    //
    // Serializes a SubgraphTemplate from the harness shared state.
    // Builds a minimal template if none exists yet.
    //
    // On success: "subgraph_serial.serialize ok bytes=<B>"
    // On error:   "subgraph_serial.serialize error: ..."
    harness.registry().registerCommand("subgraph_serial.serialize",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->subgraph) {
                state->subgraph = nexus::eval_ext::SubgraphTemplate{};
            }

            nexus::eval_ext::SubgraphSerializationReport report;
            const std::string data =
                nexus::eval_ext::SubgraphSerializer::serialize(*state->subgraph, report);

            if (!report.ok) {
                for (const auto& e : report.errors)
                    messages.push_back("subgraph_serial.serialize error: " + e);
                if (messages.empty())
                    messages.push_back("subgraph_serial.serialize error: unknown");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->serializedSubgraph    = data;
            state->hasSerializedSubgraph = true;
            messages.push_back("subgraph_serial.serialize ok"
                " bytes=" + std::to_string(data.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph_serial.deserialize ───────────────────────────────────────────
    //
    // Deserializes the last serialized subgraph back.
    // Returns false when nothing has been serialized.
    //
    // On success: "subgraph_serial.deserialize ok"
    // On error:   "subgraph_serial.deserialize error: ..."
    harness.registry().registerCommand("subgraph_serial.deserialize",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasSerializedSubgraph) {
                messages.push_back("subgraph_serial.deserialize error: nothing serialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::eval_ext::SubgraphTemplate out;
            const auto report =
                nexus::eval_ext::SubgraphSerializer::deserialize(
                    state->serializedSubgraph, out);

            if (!report.ok) {
                for (const auto& e : report.errors)
                    messages.push_back("subgraph_serial.deserialize error: " + e);
                if (messages.empty())
                    messages.push_back("subgraph_serial.deserialize error: parse failure");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->subgraph = std::move(out);
            messages.push_back("subgraph_serial.deserialize ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── subgraph_serial.describe ──────────────────────────────────────────────
    //
    // Reports subgraph serialization state.  Always returns true.
    //
    // "subgraph_serial.describe has_subgraph=<0|1> has_serialized=<0|1>"
    harness.registry().registerCommand("subgraph_serial.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("subgraph_serial.describe"
                " has_subgraph="  + std::string(state->subgraph.has_value() ? "1" : "0") +
                " has_serialized=" + std::string(state->hasSerializedSubgraph ? "1" : "0"));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
