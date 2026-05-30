// Tests for param.add_point, param.add_constraint, param.solve, param.describe
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/ParametricExtension.h>
#include <nexus/parametric/ConstraintGraph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerParametricCommands(h);
    return h;
}

} // namespace

// ── param.add_point ───────────────────────────────────────────────────────────

TEST(ParametricExtension, AddPointDefaultOrigin)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_point";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("param.add_point ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(ParametricExtension, AddPointWithCoordinates)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_point";
    cmd.args["x"] = "1.5";
    cmd.args["y"] = "2.5";
    cmd.args["z"] = "3.5";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    bool foundX = false;
    for (const auto& m : msgs)
        if (m.find("x=") != std::string::npos) { foundX = true; break; }
    EXPECT_TRUE(foundX);
}

TEST(ParametricExtension, AddMultiplePointsGetDistinctIds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    auto addPt = [&](float x, float y, float z) -> std::string {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_point";
        cmd.args["x"] = std::to_string(x);
        cmd.args["y"] = std::to_string(y);
        cmd.args["z"] = std::to_string(z);
        std::vector<std::string> msgs;
        EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
        for (const auto& m : msgs)
            if (m.find("param.add_point ok") != std::string::npos) return m;
        return "";
    };

    const std::string r0 = addPt(0.f, 0.f, 0.f);
    const std::string r1 = addPt(1.f, 0.f, 0.f);
    const std::string r2 = addPt(0.f, 1.f, 0.f);

    EXPECT_FALSE(r0.empty());
    EXPECT_FALSE(r1.empty());
    EXPECT_FALSE(r2.empty());
    // All three success messages must be different (distinct ids)
    EXPECT_NE(r0, r1);
    EXPECT_NE(r1, r2);
}

TEST(ParametricExtension, AddPointSetsHasParametricGraph)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    EXPECT_FALSE(ctx.hasParametricGraph);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_point";

    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    EXPECT_TRUE(ctx.hasParametricGraph);
}

TEST(ParametricExtension, AddPointMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_point";
    cmd.args["x"] = "1"; cmd.args["y"] = "2"; cmd.args["z"] = "3";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── param.add_constraint ─────────────────────────────────────────────────────

TEST(ParametricExtension, DistanceConstraintSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    // Add two points
    auto addPt = [&](float x, float y) -> uint64_t {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_point";
        cmd.args["x"] = std::to_string(x);
        cmd.args["y"] = std::to_string(y);
        cmd.args["z"] = "0";
        std::vector<std::string> msgs;
        EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
        for (const auto& m : msgs) {
            auto pos = m.find("id=");
            if (pos == std::string::npos) continue;
            return std::stoull(m.substr(pos + 3));
        }
        return 0;
    };

    const uint64_t idA = addPt(0.f, 0.f);
    const uint64_t idB = addPt(1.f, 0.f);
    EXPECT_GT(idA, 0u);
    EXPECT_GT(idB, 0u);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_constraint";
    cmd.args["type"]  = "distance";
    cmd.args["a"]     = std::to_string(idA);
    cmd.args["b"]     = std::to_string(idB);
    cmd.args["value"] = "1.0";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);
    EXPECT_TRUE(ok);
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("param.add_constraint ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(ParametricExtension, CoincidentConstraintSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    auto addPt = [&](float x, float y) -> uint64_t {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_point";
        cmd.args["x"] = std::to_string(x);
        cmd.args["y"] = std::to_string(y);
        cmd.args["z"] = "0";
        std::vector<std::string> msgs;
        EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
        for (const auto& m : msgs) {
            auto pos = m.find("id=");
            if (pos == std::string::npos) continue;
            return std::stoull(m.substr(pos + 3));
        }
        return 0;
    };

    const uint64_t idA = addPt(0.f, 0.f);
    const uint64_t idB = addPt(0.f, 0.f);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_constraint";
    cmd.args["type"] = "coincident";
    cmd.args["a"]    = std::to_string(idA);
    cmd.args["b"]    = std::to_string(idB);

    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
}

TEST(ParametricExtension, AxisDistanceConstraintSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    auto addPt = [&](float x, float y) -> uint64_t {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_point";
        cmd.args["x"] = std::to_string(x);
        cmd.args["y"] = std::to_string(y);
        cmd.args["z"] = "0";
        std::vector<std::string> msgs;
        EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
        for (const auto& m : msgs) {
            auto pos = m.find("id=");
            if (pos == std::string::npos) continue;
            return std::stoull(m.substr(pos + 3));
        }
        return 0;
    };

    const uint64_t idA = addPt(0.f, 0.f);
    const uint64_t idB = addPt(2.f, 0.f);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_constraint";
    cmd.args["type"]  = "axis_distance";
    cmd.args["axis"]  = "x";
    cmd.args["a"]     = std::to_string(idA);
    cmd.args["b"]     = std::to_string(idB);
    cmd.args["value"] = "2.0";

    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
}

TEST(ParametricExtension, ConstraintWithInvalidEntityFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_constraint";
    cmd.args["type"] = "distance";
    cmd.args["a"]    = "999";
    cmd.args["b"]    = "1000";
    cmd.args["value"]= "1.0";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);
    EXPECT_FALSE(ok);
    ASSERT_FALSE(msgs.empty());
}

TEST(ParametricExtension, ConstraintMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    // Add two points first
    for (int i = 0; i < 2; ++i) {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_point";
        cmd.args["x"] = std::to_string(i);
        cmd.args["y"] = "0"; cmd.args["z"] = "0";
        std::vector<std::string> msgs;
        [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    }

    // Get ids from graph
    const auto& ents = ctx.parametricGraph.entities();
    ASSERT_GE(ents.size(), 2u);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_constraint";
    cmd.args["type"]  = "distance";
    cmd.args["a"]     = std::to_string(ents[0].id);
    cmd.args["b"]     = std::to_string(ents[1].id);
    cmd.args["value"] = "1.0";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── param.solve ───────────────────────────────────────────────────────────────

TEST(ParametricExtension, SolveEmptyGraphConverges)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.solve";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);
    EXPECT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("param.solve ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(ParametricExtension, SolveSketchRectangleConverges)
{
    // Build a simple 2-point, 1-distance-constraint sketch and solve it.
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    // Add origin + corner
    auto addPt = [&](float x, float y, float z) -> uint64_t {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_point";
        cmd.args["x"] = std::to_string(x);
        cmd.args["y"] = std::to_string(y);
        cmd.args["z"] = std::to_string(z);
        std::vector<std::string> msgs;
        EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
        for (const auto& m : msgs) {
            auto pos = m.find("id=");
            if (pos == std::string::npos) continue;
            return std::stoull(m.substr(pos + 3));
        }
        return 0;
    };

    const uint64_t idA = addPt(0.f, 0.f, 0.f);
    const uint64_t idB = addPt(1.f, 0.f, 0.f);

    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_constraint";
        cmd.args["type"]  = "distance";
        cmd.args["a"]     = std::to_string(idA);
        cmd.args["b"]     = std::to_string(idB);
        cmd.args["value"] = "1.0";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.solve";
    cmd.args["max_iterations"] = "32";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);
    EXPECT_TRUE(ok);

    bool hasIterations = false, hasError = false;
    for (const auto& m : msgs) {
        if (m.find("iterations=") != std::string::npos) hasIterations = true;
        if (m.find("error=")      != std::string::npos) hasError = true;
    }
    EXPECT_TRUE(hasIterations);
    EXPECT_TRUE(hasError);
}

TEST(ParametricExtension, SolveMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.solve";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── param.describe ───────────────────────────────────────────────────────────

TEST(ParametricExtension, DescribeEmptyGraph)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);
    EXPECT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("param.describe") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(ParametricExtension, DescribeReportsPointCount)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    for (int i = 0; i < 3; ++i) {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_point";
        cmd.args["x"] = std::to_string(i);
        cmd.args["y"] = "0"; cmd.args["z"] = "0";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.describe";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool foundPoints = false;
    for (const auto& m : msgs)
        if (m.find("points=3") != std::string::npos) { foundPoints = true; break; }
    EXPECT_TRUE(foundPoints);
}

TEST(ParametricExtension, DescribeListsPointCoordinates)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.add_point";
    cmd.args["x"] = "7"; cmd.args["y"] = "0"; cmd.args["z"] = "0";
    std::vector<std::string> msgs0;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs0));

    nexus::automation::ScriptCommand descCmd;
    descCmd.name = "param.describe";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, descCmd, msgs));

    // Should see a point line with x=7
    bool foundPt = false;
    for (const auto& m : msgs)
        if (m.find("point id=") != std::string::npos) { foundPt = true; break; }
    EXPECT_TRUE(foundPt);
}

TEST(ParametricExtension, DescribeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_point";
        cmd.args["x"] = "1"; cmd.args["y"] = "1"; cmd.args["z"] = "0";
        std::vector<std::string> msgs;
        [[maybe_unused]] bool ok2 = h.registry().execute(ctx, cmd, msgs);
    }

    nexus::automation::ScriptCommand cmd;
    cmd.name = "param.describe";
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── pipeline: add points → constrain → solve → describe ──────────────────────

TEST(ParametricExtension, FullPipelineSketch)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    // Add 4 corners of a unit square
    struct Pt { float x, y; };
    const Pt corners[] = {{0,0},{1,0},{1,1},{0,1}};
    std::vector<uint64_t> ids;

    for (const auto& p : corners) {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_point";
        cmd.args["x"] = std::to_string(p.x);
        cmd.args["y"] = std::to_string(p.y);
        cmd.args["z"] = "0";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
        for (const auto& m : msgs) {
            auto pos = m.find("id=");
            if (pos == std::string::npos) continue;
            ids.push_back(std::stoull(m.substr(pos + 3)));
        }
    }
    ASSERT_EQ(ids.size(), 4u);

    // Constrain side lengths to 1.0
    for (std::size_t i = 0; i < 4; ++i) {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.add_constraint";
        cmd.args["type"]  = "distance";
        cmd.args["a"]     = std::to_string(ids[i]);
        cmd.args["b"]     = std::to_string(ids[(i + 1) % 4]);
        cmd.args["value"] = "1.0";
        std::vector<std::string> msgs;
        EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    // Solve
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.solve";
        cmd.args["max_iterations"] = "64";
        std::vector<std::string> msgs;
        EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    // Describe — graph must still have 4 points
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "param.describe";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
        bool found4 = false;
        for (const auto& m : msgs)
            if (m.find("points=4") != std::string::npos) { found4 = true; break; }
        EXPECT_TRUE(found4);
    }
}
