// ─────────────────────────────────────────────────────────────────────────────
//  Tests for AssetExtension — asset.new/add_entry/remove_entry/set_name/describe
//  and gaussian.init/add_splat/snapshot/diff/describe
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AssetExtension.h>
#include <nexus/automation/AutomationScript.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerAssetCommands(h);
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
//  asset.new
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, AssetNewDefault)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "asset.new ok name=scene"));
    EXPECT_TRUE(ctx.hasScene);
}

TEST(AssetExtension, AssetNewCustomName)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("asset.new", {{"name","myScene"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "asset.new ok name=myScene"));
    EXPECT_EQ(ctx.scene.sceneName(), "myScene");
    EXPECT_EQ(ctx.sceneName, "myScene");
}

TEST(AssetExtension, AssetNewResetsEntries)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n1 = h.registry().execute(ctx,
        makeCmd("asset.new"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","obj"}}), msgs);
    EXPECT_EQ(ctx.scene.entryCount(), 1u);

    msgs.clear();
    [[maybe_unused]] bool n2 = h.registry().execute(ctx,
        makeCmd("asset.new"), msgs);
    EXPECT_EQ(ctx.scene.entryCount(), 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  asset.add_entry
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, AddEntryNoSceneFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","x"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "asset.add_entry error:"));
}

TEST(AssetExtension, AddEntryIncrementsCount)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    msgs.clear();

    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","A"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","B"}}), msgs);
    EXPECT_EQ(ctx.scene.entryCount(), 2u);
}

TEST(AssetExtension, AddEntryIndexInMessage)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    msgs.clear();

    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","first"}}), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "asset.add_entry ok index=0 name=first"));
}

TEST(AssetExtension, AddEntryVisibilityDefault)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","v"}}), msgs);
    EXPECT_TRUE(ctx.scene.entry(0).visible);
}

TEST(AssetExtension, AddEntryVisibilityZero)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","hidden"},{"visible","0"}}), msgs);
    EXPECT_FALSE(ctx.scene.entry(0).visible);
}

// ─────────────────────────────────────────────────────────────────────────────
//  asset.remove_entry
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, RemoveEntryNoSceneFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("asset.remove_entry", {{"index","0"}}), msgs);
    EXPECT_FALSE(ok);
}

TEST(AssetExtension, RemoveEntryOutOfRangeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("asset.remove_entry", {{"index","5"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "asset.remove_entry error:"));
}

TEST(AssetExtension, RemoveEntryDecrementsCount)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n  = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","A"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","B"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("asset.remove_entry", {{"index","0"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "asset.remove_entry ok index=0 remaining=1"));
    EXPECT_EQ(ctx.scene.entryCount(), 1u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  asset.set_name
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, SetNameNoSceneFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("asset.set_name", {{"name","x"}}), msgs);
    EXPECT_FALSE(ok);
}

TEST(AssetExtension, SetNameEmptyFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("asset.set_name"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "asset.set_name error:"));
}

TEST(AssetExtension, SetNameUpdatesScene)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("asset.set_name", {{"name","renamed"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "asset.set_name ok name=renamed"));
    EXPECT_EQ(ctx.scene.sceneName(), "renamed");
    EXPECT_EQ(ctx.sceneName, "renamed");
}

// ─────────────────────────────────────────────────────────────────────────────
//  asset.describe
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, DescribeNoScene)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx, makeCmd("asset.describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "asset.describe entries=0 name="));
}

TEST(AssetExtension, DescribeWithEntries)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("asset.new", {{"name","myScene"}}), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("asset.add_entry", {{"name","mesh0"}}), msgs);
    msgs.clear();

    h.registry().execute(ctx, makeCmd("asset.describe"), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "asset.describe entries=1 name=myScene"));
    EXPECT_TRUE(hasPrefix(msgs, "asset.describe entry index=0 name=mesh0 visible=1"));
}

TEST(AssetExtension, DescribeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("asset.new"), msgs);
    for (int i = 0; i < 3; ++i) {
        [[maybe_unused]] bool a = h.registry().execute(ctx,
            makeCmd("asset.add_entry", {{"name","e"+std::to_string(i)}}), msgs);
    }
    msgs.clear();

    h.registry().execute(ctx, makeCmd("asset.describe"), msgs);
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ─────────────────────────────────────────────────────────────────────────────
//  gaussian.init
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, GaussianInitOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx, makeCmd("gaussian.init"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "gaussian.init ok sh_degree=3"));
    EXPECT_TRUE(ctx.hasGaussianCloud);
}

TEST(AssetExtension, GaussianInitCustomDegree)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool ok = h.registry().execute(ctx,
        makeCmd("gaussian.init", {{"sh_degree","1"}}), msgs);
    EXPECT_EQ(ctx.gaussianCloud.shDegree, 1u);
    EXPECT_TRUE(hasPrefix(msgs, "gaussian.init ok sh_degree=1"));
}

TEST(AssetExtension, GaussianInitClampsDegree)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool ok = h.registry().execute(ctx,
        makeCmd("gaussian.init", {{"sh_degree","9"}}), msgs);
    EXPECT_EQ(ctx.gaussianCloud.shDegree, 3u); // clamped to max
}

// ─────────────────────────────────────────────────────────────────────────────
//  gaussian.add_splat
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, AddSplatNoCloudFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("gaussian.add_splat"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "gaussian.add_splat error:"));
}

TEST(AssetExtension, AddSplatIncrementCount)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool i = h.registry().execute(ctx, makeCmd("gaussian.init"), msgs);
    msgs.clear();

    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("gaussian.add_splat", {{"px","1"},{"py","2"},{"pz","3"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("gaussian.add_splat"), msgs);
    EXPECT_EQ(ctx.gaussianCloud.splatCount(), 2u);
}

TEST(AssetExtension, AddSplatIndexInMessage)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool i = h.registry().execute(ctx, makeCmd("gaussian.init"), msgs);
    msgs.clear();

    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("gaussian.add_splat"), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "gaussian.add_splat ok index=0"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  gaussian.snapshot / gaussian.diff
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, GaussianDiffNoSnapshotFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool i = h.registry().execute(ctx, makeCmd("gaussian.init"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx, makeCmd("gaussian.diff"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "gaussian.diff error: no snapshot"));
}

TEST(AssetExtension, GaussianSnapshotThenDiff)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool i  = h.registry().execute(ctx, makeCmd("gaussian.init"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("gaussian.add_splat"), msgs);
    [[maybe_unused]] bool sn = h.registry().execute(ctx,
        makeCmd("gaussian.snapshot"), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("gaussian.add_splat"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx, makeCmd("gaussian.diff"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "gaussian.diff ok baseline=1 current=2 added=1"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  gaussian.describe
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, GaussianDescribeNoCloud)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx, makeCmd("gaussian.describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "gaussian.describe splats=0 sh_degree=0"));
}

TEST(AssetExtension, GaussianDescribeAfterAdd)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool i = h.registry().execute(ctx, makeCmd("gaussian.init"), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx, makeCmd("gaussian.add_splat"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx, makeCmd("gaussian.describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "gaussian.describe splats=1 sh_degree=3"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Full pipeline: scene + gaussian together
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetExtension, FullPipeline)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    // Build a named scene with 3 entries.
    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("asset.new", {{"name","production"}}), msgs);
    for (int i = 0; i < 3; ++i) {
        [[maybe_unused]] bool a = h.registry().execute(ctx,
            makeCmd("asset.add_entry",
                {{"name","mesh"+std::to_string(i)},{"visible","1"}}), msgs);
    }
    EXPECT_EQ(ctx.scene.entryCount(), 3u);

    // Remove the middle entry.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("asset.remove_entry", {{"index","1"}}), msgs);
        EXPECT_TRUE(ok);
        EXPECT_EQ(ctx.scene.entryCount(), 2u);
    }

    // Rename the scene.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("asset.set_name", {{"name","final"}}), msgs);
        EXPECT_TRUE(ok);
    }

    // Initialize Gaussian cloud and add 5 splats, snapshot, add 2 more, diff.
    [[maybe_unused]] bool gi = h.registry().execute(ctx,
        makeCmd("gaussian.init", {{"sh_degree","2"}}), msgs);
    for (int i = 0; i < 5; ++i) {
        [[maybe_unused]] bool ga = h.registry().execute(ctx,
            makeCmd("gaussian.add_splat",
                {{"px",std::to_string(float(i))}}), msgs);
    }
    [[maybe_unused]] bool gs = h.registry().execute(ctx,
        makeCmd("gaussian.snapshot"), msgs);
    for (int i = 0; i < 2; ++i) {
        [[maybe_unused]] bool ga = h.registry().execute(ctx,
            makeCmd("gaussian.add_splat"), msgs);
    }
    msgs.clear();

    {
        bool ok = h.registry().execute(ctx, makeCmd("gaussian.diff"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "gaussian.diff ok baseline=5 current=7 added=2"));
    }

    msgs.clear();

    // Describe both subsystems.
    {
        [[maybe_unused]] bool d1 = h.registry().execute(ctx,
            makeCmd("asset.describe"), msgs);
        [[maybe_unused]] bool d2 = h.registry().execute(ctx,
            makeCmd("gaussian.describe"), msgs);
        EXPECT_TRUE(hasPrefix(msgs, "asset.describe entries=2 name=final"));
        EXPECT_TRUE(hasPrefix(msgs, "gaussian.describe splats=7 sh_degree=2"));
    }
}
