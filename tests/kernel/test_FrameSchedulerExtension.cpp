// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation frame_sched.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/FrameSchedulerExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerFrameSchedulerCommands(h);
    return h;
}

static bool run(ScriptBatchHarness& h, const std::string& name,
                std::initializer_list<std::pair<std::string,std::string>> args,
                std::vector<std::string>& msgs)
{
    ScriptCommand cmd;
    cmd.name = name;
    for (auto& [k, v] : args) cmd.args[k] = v;
    ScriptContext ctx;
    msgs.clear();
    return h.registry().execute(ctx, cmd, msgs);
}

static bool hasMsg(const std::vector<std::string>& msgs, const std::string& sub)
{
    for (const auto& m : msgs)
        if (m.find(sub) != std::string::npos) return true;
    return false;
}

// ── frame_sched.begin ────────────────────────────────────────────────────────

TEST(FrameSchedulerExtension, BeginOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "frame_sched.begin", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame_sched.begin ok"));
    EXPECT_TRUE(hasMsg(msgs, "slot="));
    EXPECT_TRUE(hasMsg(msgs, "image="));
}

TEST(FrameSchedulerExtension, BeginOutOfDate)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "frame_sched.set_out_of_date", {}, msgs);
    EXPECT_FALSE(run(h, "frame_sched.begin", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "out_of_date"));
}

TEST(FrameSchedulerExtension, BeginAfterResizeRecovery)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "frame_sched.set_out_of_date", {}, msgs);
    // Simulate resize recovery
    ok = run(h, "frame_sched.resize", {{"width","800"},{"height","600"}}, msgs);
    EXPECT_TRUE(run(h, "frame_sched.begin", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame_sched.begin ok"));
}

TEST(FrameSchedulerExtension, BeginMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "frame_sched.begin", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── frame_sched.end ──────────────────────────────────────────────────────────

TEST(FrameSchedulerExtension, EndRequiresBegin)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "frame_sched.end", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(FrameSchedulerExtension, EndOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "frame_sched.begin", {}, msgs);
    EXPECT_TRUE(run(h, "frame_sched.end", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame_sched.end ok"));
    EXPECT_TRUE(hasMsg(msgs, "frames=1"));
}

TEST(FrameSchedulerExtension, EndIncrementsFrameCount)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    for (int i = 0; i < 3; ++i) {
        ok = run(h, "frame_sched.begin", {}, msgs);
        ok = run(h, "frame_sched.end",   {}, msgs);
    }
    EXPECT_TRUE(run(h, "frame_sched.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frames=3"));
}

TEST(FrameSchedulerExtension, EndRequiresBeginAfterPreviousEnd)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "frame_sched.begin", {}, msgs);
    ok = run(h, "frame_sched.end",   {}, msgs);
    // Second end without begin
    EXPECT_FALSE(run(h, "frame_sched.end", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

// ── frame_sched.resize ───────────────────────────────────────────────────────

TEST(FrameSchedulerExtension, ResizeOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "frame_sched.resize", {{"width","1920"},{"height","1080"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame_sched.resize ok"));
    EXPECT_TRUE(hasMsg(msgs, "width=1920"));
    EXPECT_TRUE(hasMsg(msgs, "height=1080"));
}

TEST(FrameSchedulerExtension, ResizeUpdatesDescribe)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "frame_sched.resize",
        {{"width","2560"},{"height","1440"}}, msgs);
    EXPECT_TRUE(run(h, "frame_sched.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "width=2560"));
    EXPECT_TRUE(hasMsg(msgs, "height=1440"));
}

// ── frame_sched.set_out_of_date ──────────────────────────────────────────────

TEST(FrameSchedulerExtension, SetOutOfDateOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "frame_sched.set_out_of_date", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame_sched.set_out_of_date ok"));
}

// ── frame_sched.describe ─────────────────────────────────────────────────────

TEST(FrameSchedulerExtension, DescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "frame_sched.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "max_in_flight=2"));
    EXPECT_TRUE(hasMsg(msgs, "frames=0"));
    EXPECT_TRUE(hasMsg(msgs, "width=1280"));
    EXPECT_TRUE(hasMsg(msgs, "height=720"));
}

TEST(FrameSchedulerExtension, DescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "frame_sched.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── Integration ───────────────────────────────────────────────────────────────

TEST(FrameSchedulerExtension, FullFrameLoop)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Normal frame loop — 5 frames
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(run(h, "frame_sched.begin", {}, msgs));
        EXPECT_TRUE(run(h, "frame_sched.end",   {}, msgs));
    }

    EXPECT_TRUE(run(h, "frame_sched.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frames=5"));

    // Simulate out-of-date: next begin fails
    ok = run(h, "frame_sched.set_out_of_date", {}, msgs);
    EXPECT_FALSE(run(h, "frame_sched.begin", {}, msgs));

    // Resize recovers the scheduler
    ok = run(h, "frame_sched.resize", {{"width","1920"},{"height","1080"}}, msgs);
    EXPECT_TRUE(run(h, "frame_sched.begin", {}, msgs));
    EXPECT_TRUE(run(h, "frame_sched.end",   {}, msgs));

    EXPECT_TRUE(run(h, "frame_sched.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frames=6"));
    EXPECT_TRUE(hasMsg(msgs, "width=1920"));
}

TEST(FrameSchedulerExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h1, "frame_sched.begin", {}, msgs);
    ok = run(h1, "frame_sched.end",   {}, msgs);

    EXPECT_TRUE(run(h2, "frame_sched.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frames=0"));
}
