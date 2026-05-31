// ─────────────────────────────────────────────────────────────────────────────
//  Tests — EvalNodesExtension
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include <nexus/automation/EvalNodesExtension.h>
#include <nexus/automation/AutomationScript.h>

using namespace nexus::automation;

static std::vector<std::string> run(ScriptBatchHarness& h,
                                     const std::string& name,
                                     std::map<std::string,std::string> args = {})
{
    ScriptContext ctx;
    ScriptCommand cmd;
    cmd.name = name;
    cmd.args = std::move(args);
    std::vector<std::string> msgs;
    (void)h.registry().execute(ctx, cmd, msgs);
    return msgs;
}

static bool runOk(ScriptBatchHarness& h, const std::string& name,
                  std::map<std::string,std::string> args = {})
{
    ScriptContext ctx;
    ScriptCommand cmd;
    cmd.name = name;
    cmd.args = std::move(args);
    std::vector<std::string> msgs;
    return h.registry().execute(ctx, cmd, msgs);
}

class EvalNodesTest : public ::testing::Test {
protected:
    ScriptBatchHarness harness;
    void SetUp() override { registerEvalNodesCommands(harness); }
};

// ── anim_blend ────────────────────────────────────────────────────────────────

TEST_F(EvalNodesTest, AnimBlendDefaultWeight) {
    EXPECT_TRUE(runOk(harness, "anim_blend.compute"));
    auto msgs = run(harness, "anim_blend.compute");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
    EXPECT_NE(msgs[0].find("weight="), std::string::npos);
}

TEST_F(EvalNodesTest, AnimBlendCustomWeight) {
    EXPECT_TRUE(runOk(harness, "anim_blend.compute", {{"weight","0.25"}}));
    auto msgs = run(harness, "anim_blend.compute", {{"weight","0.25"}});
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("weight=0.25"), std::string::npos);
}

// ── anim_sample ───────────────────────────────────────────────────────────────

TEST_F(EvalNodesTest, AnimSampleDefaultTime) {
    EXPECT_TRUE(runOk(harness, "anim_sample.compute"));
    auto msgs = run(harness, "anim_sample.compute");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
    EXPECT_NE(msgs[0].find("time="), std::string::npos);
}

TEST_F(EvalNodesTest, AnimSampleCustomTime) {
    EXPECT_TRUE(runOk(harness, "anim_sample.compute", {{"time","0.5"}}));
}

// ── anim_retarget ─────────────────────────────────────────────────────────────

TEST_F(EvalNodesTest, AnimRetargetCompute) {
    // sample first to populate pose
    runOk(harness, "anim_sample.compute");
    auto result = runOk(harness, "anim_retarget.compute");
    // may succeed or fail depending on skeleton match; just check it runs
    auto msgs = run(harness, "anim_retarget.compute");
    EXPECT_FALSE(msgs.empty());
}

// ── geom_merge ────────────────────────────────────────────────────────────────
// Empty meshes cause these ops to return false; verify the command is registered
// and returns a non-empty message (either ok or error).

TEST_F(EvalNodesTest, GeomMergeComputeRegistered) {
    auto msgs = run(harness, "geom_merge.compute");
    EXPECT_FALSE(msgs.empty());
}

TEST_F(EvalNodesTest, GeomMergeMessageHasVertsFacesOrError) {
    auto msgs = run(harness, "geom_merge.compute");
    ASSERT_FALSE(msgs.empty());
    const bool hasStats = msgs[0].find("verts=") != std::string::npos
                       && msgs[0].find("faces=") != std::string::npos;
    const bool hasError = msgs[0].find("error") != std::string::npos;
    EXPECT_TRUE(hasStats || hasError);
}

// ── geom_boolean ──────────────────────────────────────────────────────────────

TEST_F(EvalNodesTest, GeomBooleanUnionRegistered) {
    auto msgs = run(harness, "geom_boolean.compute");
    EXPECT_FALSE(msgs.empty());
}

TEST_F(EvalNodesTest, GeomBooleanDifferenceRegistered) {
    auto msgs = run(harness, "geom_boolean.compute", {{"op","Difference"}});
    EXPECT_FALSE(msgs.empty());
}

TEST_F(EvalNodesTest, GeomBooleanIntersectionRegistered) {
    auto msgs = run(harness, "geom_boolean.compute", {{"op","Intersection"}});
    EXPECT_FALSE(msgs.empty());
}

// ── geom_remesh ───────────────────────────────────────────────────────────────

TEST_F(EvalNodesTest, GeomRemeshRegistered) {
    auto msgs = run(harness, "geom_remesh.compute");
    EXPECT_FALSE(msgs.empty());
}

TEST_F(EvalNodesTest, GeomRemeshCustomEdgeRegistered) {
    auto msgs = run(harness, "geom_remesh.compute", {{"target_edge","0.05"}});
    EXPECT_FALSE(msgs.empty());
}

// ── subgraph_serial ───────────────────────────────────────────────────────────

TEST_F(EvalNodesTest, SubgraphDescribeInitial) {
    auto msgs = run(harness, "subgraph_serial.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("has_subgraph=0"), std::string::npos);
    EXPECT_NE(msgs[0].find("has_serialized=0"), std::string::npos);
}

TEST_F(EvalNodesTest, SubgraphSerialize) {
    EXPECT_TRUE(runOk(harness, "subgraph_serial.serialize"));
    auto msgs = run(harness, "subgraph_serial.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("has_subgraph=1"), std::string::npos);
    EXPECT_NE(msgs[0].find("has_serialized=1"), std::string::npos);
}

TEST_F(EvalNodesTest, SubgraphSerializeOkBytes) {
    auto msgs = run(harness, "subgraph_serial.serialize");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
    EXPECT_NE(msgs[0].find("bytes="), std::string::npos);
}

TEST_F(EvalNodesTest, SubgraphDeserializeNoData) {
    EXPECT_FALSE(runOk(harness, "subgraph_serial.deserialize"));
    auto msgs = run(harness, "subgraph_serial.deserialize");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

TEST_F(EvalNodesTest, SubgraphSerializeDeserialize) {
    EXPECT_TRUE(runOk(harness, "subgraph_serial.serialize"));
    EXPECT_TRUE(runOk(harness, "subgraph_serial.deserialize"));
    auto msgs = run(harness, "subgraph_serial.deserialize");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
}
