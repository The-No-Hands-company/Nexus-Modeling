// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation stroke_hist.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/StrokeHistoryExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerStrokeHistoryCommands(h);
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

// ── stroke_hist.add ───────────────────────────────────────────────────────────

TEST(StrokeHistoryExtension, AddDefaultKind)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "stroke_hist.add", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "stroke_hist.add ok"));
    EXPECT_TRUE(hasMsg(msgs, "id=1"));
    EXPECT_TRUE(hasMsg(msgs, "kind=Draw"));
}

TEST(StrokeHistoryExtension, AddSmoothKind)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "stroke_hist.add", {{"kind","Smooth"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "kind=Smooth"));
}

TEST(StrokeHistoryExtension, AddInflateKind)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "stroke_hist.add", {{"kind","Inflate"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "kind=Inflate"));
}

TEST(StrokeHistoryExtension, AddIdsIncrement)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "stroke_hist.add", {}, msgs);
    EXPECT_TRUE(run(h, "stroke_hist.add", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "id=2"));
}

TEST(StrokeHistoryExtension, AddWithVertexDelta)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "stroke_hist.add",
        {{"kind","Draw"},{"vi","5"},{"dx","0.1"},{"dy","0.2"},{"dz","0.3"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "stroke_hist.add ok"));
}

// ── stroke_hist.serialize ─────────────────────────────────────────────────────

TEST(StrokeHistoryExtension, SerializeEmpty)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "stroke_hist.serialize", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "stroke_hist.serialize ok"));
    EXPECT_TRUE(hasMsg(msgs, "strokes=0"));
}

TEST(StrokeHistoryExtension, SerializeSingleStroke)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "stroke_hist.add", {{"kind","Draw"}}, msgs);
    EXPECT_TRUE(run(h, "stroke_hist.serialize", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=1"));
    EXPECT_TRUE(hasMsg(msgs, "bytes="));
}

TEST(StrokeHistoryExtension, SerializeMultipleStrokes)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "stroke_hist.add", {{"kind","Draw"}},    msgs);
    ok = run(h, "stroke_hist.add", {{"kind","Smooth"}},  msgs);
    ok = run(h, "stroke_hist.add", {{"kind","Inflate"}}, msgs);
    EXPECT_TRUE(run(h, "stroke_hist.serialize", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=3"));
}

// ── stroke_hist.deserialize ───────────────────────────────────────────────────

TEST(StrokeHistoryExtension, DeserializeRequiresSerialized)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "stroke_hist.deserialize", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(StrokeHistoryExtension, DeserializeEmptyRoundTrip)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "stroke_hist.serialize", {}, msgs);
    EXPECT_TRUE(run(h, "stroke_hist.deserialize", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=0"));
}

TEST(StrokeHistoryExtension, DeserializeRoundTrip)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "stroke_hist.add",       {{"kind","Smooth"}}, msgs);
    ok = run(h, "stroke_hist.add",       {{"kind","Draw"}},   msgs);
    ok = run(h, "stroke_hist.serialize", {},                  msgs);
    EXPECT_TRUE(run(h, "stroke_hist.deserialize", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=2"));
}

TEST(StrokeHistoryExtension, DeserializeUpdatesHistory)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "stroke_hist.add",         {{"kind","Draw"}}, msgs);
    ok = run(h, "stroke_hist.serialize",   {},                msgs);
    ok = run(h, "stroke_hist.deserialize", {},                msgs);
    // After deserialize, history == deserialized content (1 stroke)
    EXPECT_TRUE(run(h, "stroke_hist.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=1"));
}

// ── stroke_hist.clear ─────────────────────────────────────────────────────────

TEST(StrokeHistoryExtension, ClearEmpty)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "stroke_hist.clear", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "stroke_hist.clear ok"));
}

TEST(StrokeHistoryExtension, ClearResetsHistory)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "stroke_hist.add",   {{"kind","Draw"}}, msgs);
    ok = run(h, "stroke_hist.clear", {},                msgs);
    EXPECT_TRUE(run(h, "stroke_hist.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=0"));
}

TEST(StrokeHistoryExtension, ClearResetsSerializedFlag)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "stroke_hist.add",       {{"kind","Draw"}}, msgs);
    ok = run(h, "stroke_hist.serialize", {},                msgs);
    ok = run(h, "stroke_hist.clear",     {},                msgs);
    // After clear, deserialize should fail
    EXPECT_FALSE(run(h, "stroke_hist.deserialize", {}, msgs));
}

TEST(StrokeHistoryExtension, ClearResetsIdCounter)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "stroke_hist.add",   {}, msgs);
    ok = run(h, "stroke_hist.add",   {}, msgs);
    ok = run(h, "stroke_hist.clear", {}, msgs);
    // After clear, ids restart from 1
    EXPECT_TRUE(run(h, "stroke_hist.add", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "id=1"));
}

// ── stroke_hist.describe ──────────────────────────────────────────────────────

TEST(StrokeHistoryExtension, DescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "stroke_hist.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=0"));
    EXPECT_TRUE(hasMsg(msgs, "serialized=0"));
}

TEST(StrokeHistoryExtension, DescribeAfterAdd)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "stroke_hist.add", {}, msgs);
    EXPECT_TRUE(run(h, "stroke_hist.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=1"));
}

TEST(StrokeHistoryExtension, DescribeAfterSerialize)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "stroke_hist.add",       {}, msgs);
    ok = run(h, "stroke_hist.serialize", {}, msgs);
    EXPECT_TRUE(run(h, "stroke_hist.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "serialized=1"));
}

// ── Integration ───────────────────────────────────────────────────────────────

TEST(StrokeHistoryExtension, FullRoundTrip)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Build 3-stroke history
    ok = run(h, "stroke_hist.add", {{"kind","Draw"},  {"vi","0"},{"dx","0.5"},{"dy","0.0"},{"dz","0.0"}}, msgs);
    ok = run(h, "stroke_hist.add", {{"kind","Smooth"},{"vi","1"},{"dx","0.0"},{"dy","0.3"},{"dz","0.0"}}, msgs);
    ok = run(h, "stroke_hist.add", {{"kind","Inflate"}}, msgs);

    EXPECT_TRUE(run(h, "stroke_hist.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=3"));

    // Serialize
    EXPECT_TRUE(run(h, "stroke_hist.serialize", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=3"));

    // Deserialize — should recover 3 strokes
    EXPECT_TRUE(run(h, "stroke_hist.deserialize", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=3"));

    EXPECT_TRUE(run(h, "stroke_hist.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=3"));

    // Clear and verify reset
    ok = run(h, "stroke_hist.clear", {}, msgs);
    EXPECT_TRUE(run(h, "stroke_hist.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=0"));
    EXPECT_TRUE(hasMsg(msgs, "serialized=0"));
}

TEST(StrokeHistoryExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h1, "stroke_hist.add", {}, msgs);

    EXPECT_TRUE(run(h2, "stroke_hist.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "strokes=0"));
}
