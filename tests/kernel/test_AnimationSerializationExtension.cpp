// ─────────────────────────────────────────────────────────────────────────────
//  Tests — AnimationSerializationExtension
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include <nexus/automation/AnimationSerializationExtension.h>
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

class AnimSerialTest : public ::testing::Test {
protected:
    ScriptBatchHarness harness;
    void SetUp() override { registerAnimationSerializationCommands(harness); }
};

TEST_F(AnimSerialTest, DescribeInitial) {
    auto msgs = run(harness, "anim_serial.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("has_clip=0"), std::string::npos);
}

TEST_F(AnimSerialTest, MakeClipDefault) {
    EXPECT_TRUE(runOk(harness, "anim_serial.make_clip"));
    auto msgs = run(harness, "anim_serial.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("has_clip=1"), std::string::npos);
}

TEST_F(AnimSerialTest, MakeClipCustomDuration) {
    EXPECT_TRUE(runOk(harness, "anim_serial.make_clip", {{"duration","2.0"},{"rate","60.0"}}));
    auto msgs = run(harness, "anim_serial.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("has_clip=1"), std::string::npos);
    EXPECT_NE(msgs[0].find("rate=60"), std::string::npos);
}

TEST_F(AnimSerialTest, MakeClipLoopWrap) {
    EXPECT_TRUE(runOk(harness, "anim_serial.make_clip", {{"wrap","Loop"}}));
}

TEST_F(AnimSerialTest, SaveNoClip) {
    EXPECT_FALSE(runOk(harness, "anim_serial.save", {{"path","/tmp/test_anim.nxa"}}));
    auto msgs = run(harness, "anim_serial.save", {{"path","/tmp/test_anim.nxa"}});
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

TEST_F(AnimSerialTest, SaveMissingPath) {
    EXPECT_TRUE(runOk(harness, "anim_serial.make_clip"));
    EXPECT_FALSE(runOk(harness, "anim_serial.save"));
    auto msgs = run(harness, "anim_serial.save");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

TEST_F(AnimSerialTest, SaveAndLoad) {
    EXPECT_TRUE(runOk(harness, "anim_serial.make_clip", {{"duration","1.5"},{"rate","24.0"}}));
    EXPECT_TRUE(runOk(harness, "anim_serial.save", {{"path","/tmp/nexus_anim_test.nxa"}}));
    auto saveMsgs = run(harness, "anim_serial.save", {{"path","/tmp/nexus_anim_test.nxa"}});
    ASSERT_FALSE(saveMsgs.empty());
    EXPECT_NE(saveMsgs[0].find("ok"), std::string::npos);

    EXPECT_TRUE(runOk(harness, "anim_serial.load", {{"path","/tmp/nexus_anim_test.nxa"}}));
    auto loadMsgs = run(harness, "anim_serial.load", {{"path","/tmp/nexus_anim_test.nxa"}});
    ASSERT_FALSE(loadMsgs.empty());
    EXPECT_NE(loadMsgs[0].find("ok"), std::string::npos);
}

TEST_F(AnimSerialTest, LoadNoPath) {
    EXPECT_FALSE(runOk(harness, "anim_serial.load"));
    auto msgs = run(harness, "anim_serial.load");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

TEST_F(AnimSerialTest, LoadUsesLastPath) {
    EXPECT_TRUE(runOk(harness, "anim_serial.make_clip"));
    EXPECT_TRUE(runOk(harness, "anim_serial.save", {{"path","/tmp/nexus_lastpath.nxa"}}));
    // load without path arg should use last saved path
    EXPECT_TRUE(runOk(harness, "anim_serial.load"));
}

TEST_F(AnimSerialTest, DescribeAfterLoad) {
    EXPECT_TRUE(runOk(harness, "anim_serial.make_clip", {{"duration","3.0"},{"rate","30.0"}}));
    EXPECT_TRUE(runOk(harness, "anim_serial.save", {{"path","/tmp/nexus_desc.nxa"}}));
    EXPECT_TRUE(runOk(harness, "anim_serial.load", {{"path","/tmp/nexus_desc.nxa"}}));
    auto msgs = run(harness, "anim_serial.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("has_clip=1"), std::string::npos);
}
