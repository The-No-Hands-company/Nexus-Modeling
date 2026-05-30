// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Animation extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AnimationExtension.h>
#include <nexus/animation/AnimationCore.h>
#include <nexus/animation/SkeletonRetargeter.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

struct RetargetState {
    nexus::animation::Skeleton      targetSkeleton;
    nexus::animation::SkeletonMapping mapping;
    bool hasTarget  = false;
    bool hasMapping = false;
};

[[nodiscard]] static std::optional<std::string_view>
animGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = animGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerAnimationCommands(ScriptBatchHarness& harness)
{
    auto retargetState = std::make_shared<RetargetState>();
    // ── anim.add_bone ─────────────────────────────────────────────────────────
    //
    // Appends a bone to ctx.skeleton.
    //
    // Arguments:
    //   name=<str>         bone name (default "bone")
    //   parent=<name|none> parent bone name or "none" for root (default "none")
    //   tx=<f> ty=<f> tz=<f>   bind-local translation (default 0 0 0)
    //
    // On success: "anim.add_bone ok index=<I> name=<N> parent=<P>"
    // On error:   "anim.add_bone error: ..."
    harness.registry().registerCommand("anim.add_bone",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            std::string name = "bone";
            if (const auto v = animGetArg(cmd, "name"))
                name = std::string(*v);

            std::string parentName = "none";
            if (const auto v = animGetArg(cmd, "parent"))
                parentName = std::string(*v);

            int32_t parentIndex = nexus::animation::Skeleton::kInvalidBone;
            if (parentName != "none") {
                parentIndex = ctx.skeleton.findBoneIndexByName(parentName);
                if (parentIndex == nexus::animation::Skeleton::kInvalidBone) {
                    messages.push_back("anim.add_bone error: parent bone not found: " + parentName);
                    std::sort(messages.begin(), messages.end());
                    return false;
                }
            }

            const float tx = floatArg(cmd, "tx", 0.f);
            const float ty = floatArg(cmd, "ty", 0.f);
            const float tz = floatArg(cmd, "tz", 0.f);

            nexus::animation::BoneDesc desc;
            desc.name        = name;
            desc.parentIndex = parentIndex;
            desc.bindLocal.translation = {tx, ty, tz};

            const int32_t idx = ctx.skeleton.addBone(std::move(desc));
            ctx.hasSkeleton = true;

            // Keep pose in sync with skeleton bone count.
            ctx.pose.resize(ctx.skeleton.boneCount());

            messages.push_back("anim.add_bone ok"
                " index="  + std::to_string(idx) +
                " name="   + name +
                " parent=" + parentName);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim.pose_bone ────────────────────────────────────────────────────────
    //
    // Sets the local-space translation of the named or indexed bone in ctx.pose.
    //
    // Arguments:
    //   bone=<name|index>   bone identifier (default "0")
    //   tx=<f> ty=<f> tz=<f>  local translation (default 0 0 0)
    //
    // On success: "anim.pose_bone ok index=<I>"
    // On error:   "anim.pose_bone error: ..."
    harness.registry().registerCommand("anim.pose_bone",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasSkeleton || ctx.skeleton.boneCount() == 0) {
                messages.push_back("anim.pose_bone error: skeleton is empty");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            std::string boneArg = "0";
            if (const auto v = animGetArg(cmd, "bone"))
                boneArg = std::string(*v);

            // Try numeric index first, then name lookup.
            int32_t boneIndex = -1;
            {
                uint32_t parsed = 0;
                auto [ptr, ec] = std::from_chars(
                    boneArg.data(), boneArg.data() + boneArg.size(), parsed);
                if (ec == std::errc{} && ptr == boneArg.data() + boneArg.size())
                    boneIndex = static_cast<int32_t>(parsed);
            }
            if (boneIndex < 0)
                boneIndex = ctx.skeleton.findBoneIndexByName(boneArg);

            if (boneIndex < 0 ||
                static_cast<size_t>(boneIndex) >= ctx.skeleton.boneCount()) {
                messages.push_back("anim.pose_bone error: bone not found: " + boneArg);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const float tx = floatArg(cmd, "tx", 0.f);
            const float ty = floatArg(cmd, "ty", 0.f);
            const float tz = floatArg(cmd, "tz", 0.f);

            nexus::animation::Transform t = nexus::animation::Transform::identity();
            t.translation = {tx, ty, tz};
            ctx.pose.setLocalTransform(static_cast<size_t>(boneIndex), t);

            messages.push_back("anim.pose_bone ok index=" + std::to_string(boneIndex));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim.compute_matrices ─────────────────────────────────────────────────
    //
    // Evaluates ctx.pose using ctx.skeleton to produce model-space matrices.
    // Informational — always returns true.
    //
    // On success: "anim.compute_matrices ok bones=<N> matrices=<M>"
    harness.registry().registerCommand("anim.compute_matrices",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasSkeleton || ctx.skeleton.boneCount() == 0) {
                messages.push_back("anim.compute_matrices ok bones=0 matrices=0");
                std::sort(messages.begin(), messages.end());
                return true;
            }

            ctx.pose.resize(ctx.skeleton.boneCount());
            ctx.pose.computeModelMatrices(ctx.skeleton);

            const size_t mCount = ctx.pose.modelMatrices().size();
            messages.push_back("anim.compute_matrices ok"
                " bones="   + std::to_string(ctx.skeleton.boneCount()) +
                " matrices=" + std::to_string(mCount));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim.snapshot ─────────────────────────────────────────────────────────
    //
    // Saves ctx.skeleton and ctx.pose into the animation baseline for later diff.
    // Always returns true.
    //
    // On success: "anim.snapshot ok bones=<N>"
    harness.registry().registerCommand("anim.snapshot",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            ctx.skeletonBaseline = ctx.skeleton;
            ctx.poseBaseline     = ctx.pose;
            ctx.hasAnimBaseline  = true;

            messages.push_back("anim.snapshot ok bones=" +
                std::to_string(ctx.skeleton.boneCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim.diff ─────────────────────────────────────────────────────────────
    //
    // Compares current skeleton bone count against the baseline.
    // Returns false when no baseline has been taken.
    //
    // On success: "anim.diff ok baseline=<B> current=<C> added=<A>"
    // On error:   "anim.diff error: no baseline"
    harness.registry().registerCommand("anim.diff",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasAnimBaseline) {
                messages.push_back("anim.diff error: no baseline");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const size_t baseline = ctx.skeletonBaseline.boneCount();
            const size_t current  = ctx.skeleton.boneCount();
            const size_t added    = (current >= baseline) ? (current - baseline) : 0;

            messages.push_back("anim.diff ok"
                " baseline=" + std::to_string(baseline) +
                " current="  + std::to_string(current) +
                " added="    + std::to_string(added));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim.describe ─────────────────────────────────────────────────────────
    //
    // Reports skeleton bone count, model-matrix count, and per-bone name/parent
    // for up to 12 bones.  Always returns true.
    harness.registry().registerCommand("anim.describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            const size_t nBones   = ctx.skeleton.boneCount();
            const size_t nMatrices = ctx.pose.modelMatrices().size();

            messages.push_back("anim.describe bones=" + std::to_string(nBones) +
                               " matrices=" + std::to_string(nMatrices));

            const size_t limit = std::min(nBones, size_t{12});
            for (size_t i = 0; i < limit; ++i) {
                const int32_t par = ctx.skeleton.parentIndex(i);
                std::string parentStr = (par == nexus::animation::Skeleton::kInvalidBone)
                    ? "none"
                    : ctx.skeleton.boneName(static_cast<size_t>(par));
                messages.push_back("anim.describe bone"
                    " index="  + std::to_string(i) +
                    " name="   + ctx.skeleton.boneName(i) +
                    " parent=" + parentStr);
            }

            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim.build_mapping ────────────────────────────────────────────────────
    //
    // Builds a SkeletonMapping by name from ctx.skeleton (source) to a target
    // skeleton that must first be populated via anim.add_bone into
    // ctx.skeletonBaseline (we reuse the baseline skeleton for convenience).
    //
    // This command copies ctx.skeleton as source and ctx.skeletonBaseline as
    // target, then builds a by-name mapping and stores it in retargetState.
    //
    // On success: "anim.build_mapping ok mapped=<M> source_bones=<S> target_bones=<T>"
    // On error:   "anim.build_mapping error: ..."
    harness.registry().registerCommand("anim.build_mapping",
        [retargetState](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
                        std::vector<std::string>& messages) -> bool {
            if (!ctx.hasSkeleton || ctx.skeleton.boneCount() == 0) {
                messages.push_back("anim.build_mapping error: source skeleton empty");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.hasAnimBaseline || ctx.skeletonBaseline.boneCount() == 0) {
                messages.push_back("anim.build_mapping error: target skeleton empty (use anim.snapshot to set target)");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            retargetState->targetSkeleton = ctx.skeletonBaseline;
            retargetState->mapping = nexus::animation::SkeletonMapping::buildByName(
                ctx.skeleton, retargetState->targetSkeleton);
            retargetState->hasTarget  = true;
            retargetState->hasMapping = true;

            messages.push_back("anim.build_mapping ok"
                " mapped="        + std::to_string(retargetState->mapping.mappingCount()) +
                " source_bones="  + std::to_string(ctx.skeleton.boneCount()) +
                " target_bones="  + std::to_string(retargetState->targetSkeleton.boneCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim.retarget ─────────────────────────────────────────────────────────
    //
    // Applies the current SkeletonMapping (built via anim.build_mapping) to
    // transfer ctx.pose (source) onto retargetState.targetSkeleton, writing
    // the result back into ctx.pose.  Sets ctx.skeleton to the target skeleton
    // so subsequent commands work on the retargeted rig.
    //
    // On success: "anim.retarget ok target_bones=<T>"
    // On error:   "anim.retarget error: ..."
    harness.registry().registerCommand("anim.retarget",
        [retargetState](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
                        std::vector<std::string>& messages) -> bool {
            if (!retargetState->hasMapping) {
                messages.push_back("anim.retarget error: no mapping (run anim.build_mapping first)");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.hasSkeleton || ctx.skeleton.boneCount() == 0) {
                messages.push_back("anim.retarget error: source skeleton empty");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::animation::Pose outPose;
            nexus::animation::SkeletonRetargeter::retarget(
                ctx.pose, ctx.skeleton,
                retargetState->mapping,
                retargetState->targetSkeleton,
                outPose);

            // Promote target skeleton + retargeted pose into ctx
            ctx.skeleton    = retargetState->targetSkeleton;
            ctx.pose        = std::move(outPose);
            ctx.hasSkeleton = true;

            messages.push_back("anim.retarget ok"
                " target_bones=" + std::to_string(ctx.skeleton.boneCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
