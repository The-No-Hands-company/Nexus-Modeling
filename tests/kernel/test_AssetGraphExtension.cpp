// ─────────────────────────────────────────────────────────────────────────────
//  Tests — AssetGraphExtension
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include <nexus/automation/AssetGraphExtension.h>
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

class AssetGraphTest : public ::testing::Test {
protected:
    ScriptBatchHarness harness;
    void SetUp() override { registerAssetGraphCommands(harness); }
};

TEST_F(AssetGraphTest, DescribeEmpty) {
    auto msgs = run(harness, "asset_dep.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("count=0"), std::string::npos);
}

TEST_F(AssetGraphTest, AddAsset) {
    EXPECT_TRUE(runOk(harness, "asset_dep.add", {{"path","a.mesh"},{"name","A"}}));
    auto msgs = run(harness, "asset_dep.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("count=1"), std::string::npos);
}

TEST_F(AssetGraphTest, AddMissingPath) {
    EXPECT_FALSE(runOk(harness, "asset_dep.add"));
    auto msgs = run(harness, "asset_dep.add");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

TEST_F(AssetGraphTest, AddDuplicate) {
    EXPECT_TRUE(runOk(harness, "asset_dep.add", {{"path","dup.mesh"}}));
    EXPECT_FALSE(runOk(harness, "asset_dep.add", {{"path","dup.mesh"}}));
}

TEST_F(AssetGraphTest, AddDependency) {
    EXPECT_TRUE(runOk(harness, "asset_dep.add", {{"path","a.mesh"}}));
    EXPECT_TRUE(runOk(harness, "asset_dep.add", {{"path","b.mesh"}}));
    EXPECT_TRUE(runOk(harness, "asset_dep.add_dep", {{"path","a.mesh"},{"depends_on","b.mesh"}}));
    auto msgs = run(harness, "asset_dep.add_dep", {{"path","a.mesh"},{"depends_on","b.mesh"}});
    // should fail as edge already exists
    EXPECT_FALSE(runOk(harness, "asset_dep.add_dep", {{"path","a.mesh"},{"depends_on","b.mesh"}}));
}

TEST_F(AssetGraphTest, AddDepMissingArgs) {
    EXPECT_FALSE(runOk(harness, "asset_dep.add_dep", {{"path","a.mesh"}}));
    EXPECT_FALSE(runOk(harness, "asset_dep.add_dep", {{"depends_on","b.mesh"}}));
    EXPECT_FALSE(runOk(harness, "asset_dep.add_dep"));
}

TEST_F(AssetGraphTest, RemoveAsset) {
    EXPECT_TRUE(runOk(harness, "asset_dep.add", {{"path","del.mesh"}}));
    EXPECT_TRUE(runOk(harness, "asset_dep.remove", {{"path","del.mesh"}}));
    auto msgs = run(harness, "asset_dep.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("count=0"), std::string::npos);
}

TEST_F(AssetGraphTest, RemoveNotFound) {
    EXPECT_FALSE(runOk(harness, "asset_dep.remove", {{"path","ghost.mesh"}}));
    auto msgs = run(harness, "asset_dep.remove", {{"path","ghost.mesh"}});
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

TEST_F(AssetGraphTest, ResolveEmpty) {
    EXPECT_TRUE(runOk(harness, "asset_dep.resolve"));
    auto msgs = run(harness, "asset_dep.resolve");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
}

TEST_F(AssetGraphTest, ResolveSuccess) {
    EXPECT_TRUE(runOk(harness, "asset_dep.add", {{"path","c.mesh"}}));
    EXPECT_TRUE(runOk(harness, "asset_dep.add", {{"path","d.mesh"}}));
    EXPECT_TRUE(runOk(harness, "asset_dep.add_dep", {{"path","c.mesh"},{"depends_on","d.mesh"}}));
    EXPECT_TRUE(runOk(harness, "asset_dep.resolve"));
    auto msgs = run(harness, "asset_dep.resolve");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("Success"), std::string::npos);
}

TEST_F(AssetGraphTest, ClearResetsGraph) {
    EXPECT_TRUE(runOk(harness, "asset_dep.add", {{"path","e.mesh"}}));
    EXPECT_TRUE(runOk(harness, "asset_dep.clear"));
    auto msgs = run(harness, "asset_dep.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("count=0"), std::string::npos);
}

TEST_F(AssetGraphTest, ClearOkMessage) {
    auto msgs = run(harness, "asset_dep.clear");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
}
