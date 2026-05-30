// ─────────────────────────────────────────────────────────────────────────────
//  Tests for AnimationExtension — anim.add_bone, anim.pose_bone,
//  anim.compute_matrices, anim.snapshot, anim.diff, anim.describe
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AnimationExtension.h>
#include <nexus/automation/AutomationScript.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerAnimationCommands(h);
    return h;
}

nexus::automation::ScriptCommand makeCmd(
    std::string name,
    std::vector<std::pair<std::string,std::string>> args = {})
{
    nexus::automation::ScriptCommand cmd;
    cmd.name = std::move(name);
    for (auto& [k, v] : args)
        cmd.args[k] = v;
    return cmd;
}

bool hasPrefix(const std::vector<std::string>& msgs, const std::string& prefix)
{
    return std::any_of(msgs.begin(), msgs.end(),
        [&](const std::string& m){ return m.rfind(prefix, 0) == 0; });
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  anim.add_bone
// ─────────────────────────────────────────────────────────────────────────────

TEST(AnimationExtension, AddBoneDefaultName)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.add_bone"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.add_bone ok"));
}

TEST(AnimationExtension, AddBoneSetsHasSkeleton)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    EXPECT_FALSE(ctx.hasSkeleton);
    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    EXPECT_TRUE(ctx.hasSkeleton);
}

TEST(AnimationExtension, AddBoneIncrementsBoneCount)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    EXPECT_EQ(ctx.skeleton.boneCount(), 1u);

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "spine"}, {"parent", "root"}}), msgs);
    EXPECT_EQ(ctx.skeleton.boneCount(), 2u);
}

TEST(AnimationExtension, AddBoneIndexInMessage)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "anim.add_bone ok index=0"));
}

TEST(AnimationExtension, AddBoneUnknownParentFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "child"}, {"parent", "ghost"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.add_bone error:"));
}

TEST(AnimationExtension, AddBoneTranslationStored)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"},
                                   {"tx", "1"}, {"ty", "2"}, {"tz", "3"}}), msgs);
    EXPECT_EQ(ctx.skeleton.boneCount(), 1u);
    // bindLocal translation stored on BoneDesc; just check it didn't fail
    EXPECT_TRUE(hasPrefix(msgs, "anim.add_bone ok"));
}

TEST(AnimationExtension, AddBoneRootParentNone)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "anim.add_bone ok index=0 name=root parent=none"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  anim.pose_bone
// ─────────────────────────────────────────────────────────────────────────────

TEST(AnimationExtension, PoseBoneEmptySkeletonFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.pose_bone", {{"bone", "0"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.pose_bone error:"));
}

TEST(AnimationExtension, PoseBoneByIndex)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.pose_bone", {{"bone", "0"}, {"tx", "1"}, {"ty", "2"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.pose_bone ok index=0"));
}

TEST(AnimationExtension, PoseBoneByName)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.pose_bone", {{"bone", "root"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.pose_bone ok index=0"));
}

TEST(AnimationExtension, PoseBoneInvalidNameFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.pose_bone", {{"bone", "ghost"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.pose_bone error:"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  anim.compute_matrices
// ─────────────────────────────────────────────────────────────────────────────

TEST(AnimationExtension, ComputeMatricesEmptySkeletonOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.compute_matrices"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.compute_matrices ok bones=0 matrices=0"));
}

TEST(AnimationExtension, ComputeMatricesPopulatesMatrices)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "spine"}, {"parent", "root"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.compute_matrices"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.compute_matrices ok bones=2 matrices=2"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  anim.snapshot / anim.diff
// ─────────────────────────────────────────────────────────────────────────────

TEST(AnimationExtension, DiffNoBaselineFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.diff"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.diff error: no baseline"));
}

TEST(AnimationExtension, SnapshotThenDiff)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    // Add one bone, snapshot, add another, diff.
    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    h.registry().execute(ctx,
        makeCmd("anim.snapshot"), msgs);
    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "spine"}, {"parent", "root"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.diff"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.diff ok baseline=1 current=2 added=1"));
}

TEST(AnimationExtension, SnapshotReturnsOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.snapshot"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.snapshot ok bones=0"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  anim.describe
// ─────────────────────────────────────────────────────────────────────────────

TEST(AnimationExtension, DescribeEmptySkeleton)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("anim.describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "anim.describe bones=0 matrices=0"));
}

TEST(AnimationExtension, DescribeAfterAddBone)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    msgs.clear();

    h.registry().execute(ctx,
        makeCmd("anim.describe"), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "anim.describe bones=1"));
    EXPECT_TRUE(hasPrefix(msgs, "anim.describe bone index=0 name=root parent=none"));
}

TEST(AnimationExtension, DescribeChainParents)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "hip"}}), msgs);
    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "spine"}, {"parent", "hip"}}), msgs);
    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "chest"}, {"parent", "spine"}}), msgs);
    msgs.clear();

    h.registry().execute(ctx,
        makeCmd("anim.describe"), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "anim.describe bones=3"));
    EXPECT_TRUE(hasPrefix(msgs, "anim.describe bone index=1 name=spine parent=hip"));
    EXPECT_TRUE(hasPrefix(msgs, "anim.describe bone index=2 name=chest parent=spine"));
}

TEST(AnimationExtension, DescribeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "root"}}), msgs);
    h.registry().execute(ctx,
        makeCmd("anim.add_bone", {{"name", "child"}, {"parent", "root"}}), msgs);
    msgs.clear();

    h.registry().execute(ctx,
        makeCmd("anim.describe"), msgs);
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Full pipeline: 5-bone humanoid spine
// ─────────────────────────────────────────────────────────────────────────────

TEST(AnimationExtension, HumanoidSpinePipeline)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    // Build hip -> spine -> chest -> neck -> head chain.
    const std::vector<std::pair<std::string,std::string>> chain = {
        {"hip",   "none"},
        {"spine", "hip"},
        {"chest", "spine"},
        {"neck",  "chest"},
        {"head",  "neck"},
    };
    for (const auto& [name, parent] : chain) {
        std::vector<std::pair<std::string,std::string>> args = {{"name", name}};
        if (parent != "none") args.push_back({"parent", parent});
        h.registry().execute(ctx, makeCmd("anim.add_bone", args), msgs);
    }
    EXPECT_EQ(ctx.skeleton.boneCount(), 5u);

    // Snapshot after 5 bones.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("anim.snapshot"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "anim.snapshot ok bones=5"));
    }

    // Pose the chest.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("anim.pose_bone", {{"bone", "chest"}, {"ty", "2"}}), msgs);
        EXPECT_TRUE(ok);
    }

    // Compute model matrices.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("anim.compute_matrices"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "anim.compute_matrices ok bones=5 matrices=5"));
    }

    // Diff — no new bones added since snapshot.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("anim.diff"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "anim.diff ok baseline=5 current=5 added=0"));
    }

    // Describe shows 5 bones.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("anim.describe"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "anim.describe bones=5"));
    }
}
