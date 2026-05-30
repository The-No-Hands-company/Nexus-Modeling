// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation render.graph.* and render.taa.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/RenderExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerRenderCommands(h);
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

// ── render.graph.record ───────────────────────────────────────────────────────

TEST(RenderExtension, GraphRecordDefaultPass)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.graph.record", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "render.graph.record ok"));
    EXPECT_TRUE(hasMsg(msgs, "pass=composite"));
    EXPECT_TRUE(hasMsg(msgs, "index=0"));
}

TEST(RenderExtension, GraphRecordShadowPass)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.graph.record", {{"pass","shadow"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "pass=shadow"));
    EXPECT_TRUE(hasMsg(msgs, "index=0"));
}

TEST(RenderExtension, GraphRecordGeometryPass)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.graph.record",
        {{"pass","geometry"},{"gbuffer","color_attachment"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "pass=geometry"));
    EXPECT_TRUE(hasMsg(msgs, "index=0"));
}

TEST(RenderExtension, GraphRecordMultiplePasses)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.graph.record", {{"pass","shadow"}}, msgs);
    ok = run(h, "render.graph.record", {{"pass","geometry"}}, msgs);
    EXPECT_TRUE(run(h, "render.graph.record", {{"pass","composite"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "index=2"));
}

// ── render.graph.clear ────────────────────────────────────────────────────────

TEST(RenderExtension, GraphClearEmpty)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.graph.clear", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "render.graph.clear ok"));
}

TEST(RenderExtension, GraphClearAfterRecords)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.graph.record", {{"pass","shadow"}}, msgs);
    ok = run(h, "render.graph.record", {{"pass","geometry"}}, msgs);
    ok = run(h, "render.graph.clear", {}, msgs);
    EXPECT_TRUE(run(h, "render.graph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=0"));
}

// ── render.graph.validate ─────────────────────────────────────────────────────

TEST(RenderExtension, GraphValidateEmpty)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    // An empty graph has no ordering violations — should validate ok
    EXPECT_TRUE(run(h, "render.graph.validate", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=0"));
}

TEST(RenderExtension, GraphValidateValidGraph)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.graph.record",
        {{"pass","shadow"},{"shadow_atlas","depth_write"}}, msgs);
    ok = run(h, "render.graph.record",
        {{"pass","geometry"},{"gbuffer","color_attachment"},{"shadow_atlas","depth_read"}}, msgs);
    ok = run(h, "render.graph.record",
        {{"pass","composite"},{"gbuffer","shader_read"},{"shadow_atlas","depth_read"}}, msgs);
    EXPECT_TRUE(run(h, "render.graph.validate", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "render.graph.validate ok"));
    EXPECT_TRUE(hasMsg(msgs, "passes=3"));
}

TEST(RenderExtension, GraphValidateFailsGeometryBeforeShadow)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.graph.record",
        {{"pass","geometry"},{"gbuffer","color_attachment"}}, msgs);
    ok = run(h, "render.graph.record", {{"pass","shadow"}}, msgs);
    EXPECT_FALSE(run(h, "render.graph.validate", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "render.graph.validate fail"));
}

TEST(RenderExtension, GraphValidateReportsIssueCount)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.graph.record", {{"pass","geometry"}}, msgs);
    EXPECT_FALSE(run(h, "render.graph.validate", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "issues="));
}

// ── render.graph.describe ─────────────────────────────────────────────────────

TEST(RenderExtension, GraphDescribeEmpty)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.graph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=0"));
}

TEST(RenderExtension, GraphDescribePerPass)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.graph.record",
        {{"pass","shadow"},{"shadow_atlas","depth_write"}}, msgs);
    ok = run(h, "render.graph.record",
        {{"pass","geometry"},{"gbuffer","color_attachment"}}, msgs);
    EXPECT_TRUE(run(h, "render.graph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=2"));
    EXPECT_TRUE(hasMsg(msgs, "index=0"));
    EXPECT_TRUE(hasMsg(msgs, "type=shadow"));
    EXPECT_TRUE(hasMsg(msgs, "index=1"));
    EXPECT_TRUE(hasMsg(msgs, "type=geometry"));
}

TEST(RenderExtension, GraphDescribeLayouts)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.graph.record",
        {{"pass","composite"},{"gbuffer","shader_read"},{"shadow_atlas","depth_read"}}, msgs);
    EXPECT_TRUE(run(h, "render.graph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gbuffer=shader_read"));
    EXPECT_TRUE(hasMsg(msgs, "shadow_atlas=depth_read"));
}

// ── render.taa.configure ──────────────────────────────────────────────────────

TEST(RenderExtension, TaaConfigureDefaults)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.taa.configure", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "render.taa.configure ok"));
    EXPECT_TRUE(hasMsg(msgs, "blend="));
    EXPECT_TRUE(hasMsg(msgs, "samples="));
    EXPECT_TRUE(hasMsg(msgs, "jitter="));
}

TEST(RenderExtension, TaaConfigureBlend)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.taa.configure", {{"blend","0.2"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "blend=0.200"));
}

TEST(RenderExtension, TaaConfigureSamples)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.taa.configure", {{"samples","16"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "samples=16"));
}

TEST(RenderExtension, TaaConfigureJitterUniform)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.taa.configure", {{"jitter","uniform"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "jitter=uniform"));
}

TEST(RenderExtension, TaaConfigureJitterHalton)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.taa.configure", {{"jitter","uniform"}}, msgs);
    EXPECT_TRUE(run(h, "render.taa.configure", {{"jitter","halton"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "jitter=halton"));
}

// ── render.taa.advance ────────────────────────────────────────────────────────

TEST(RenderExtension, TaaAdvanceDefault)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.taa.advance", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "render.taa.advance ok"));
    EXPECT_TRUE(hasMsg(msgs, "frame=1"));
}

TEST(RenderExtension, TaaAdvanceMultipleFrames)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.taa.advance", {{"frames","5"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame=5"));
}

TEST(RenderExtension, TaaAdvanceAccumulates)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.taa.advance", {{"frames","3"}}, msgs);
    EXPECT_TRUE(run(h, "render.taa.advance", {{"frames","4"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame=7"));
}

// ── render.taa.describe ───────────────────────────────────────────────────────

TEST(RenderExtension, TaaDescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "render.taa.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "render.taa.describe"));
    EXPECT_TRUE(hasMsg(msgs, "frame=0"));
    EXPECT_TRUE(hasMsg(msgs, "blend="));
    EXPECT_TRUE(hasMsg(msgs, "jitter_x="));
    EXPECT_TRUE(hasMsg(msgs, "jitter_y="));
}

TEST(RenderExtension, TaaDescribeAfterAdvance)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.taa.advance", {{"frames","10"}}, msgs);
    EXPECT_TRUE(run(h, "render.taa.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame=10"));
}

TEST(RenderExtension, TaaDescribeAfterConfigure)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "render.taa.configure", {{"blend","0.5"}}, msgs);
    EXPECT_TRUE(run(h, "render.taa.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "blend=0.500"));
}

// ── Integration: full deferred pipeline ──────────────────────────────────────

TEST(RenderExtension, FullDeferredPipeline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Configure TAA
    ok = run(h, "render.taa.configure",
        {{"blend","0.1"},{"samples","8"},{"jitter","halton"}}, msgs);

    // Record shadow pass
    ok = run(h, "render.graph.record",
        {{"pass","shadow"},{"shadow_atlas","depth_write"}}, msgs);

    // Record geometry pass
    ok = run(h, "render.graph.record",
        {{"pass","geometry"},{"gbuffer","color_attachment"},{"shadow_atlas","depth_read"}}, msgs);

    // Record composite pass
    ok = run(h, "render.graph.record",
        {{"pass","composite"},{"gbuffer","shader_read"},{"shadow_atlas","depth_read"}}, msgs);

    // Validate — should be ok
    EXPECT_TRUE(run(h, "render.graph.validate", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=3"));

    // Describe graph
    ok = run(h, "render.graph.describe", {}, msgs);
    EXPECT_TRUE(hasMsg(msgs, "type=shadow"));
    EXPECT_TRUE(hasMsg(msgs, "type=geometry"));
    EXPECT_TRUE(hasMsg(msgs, "type=composite"));

    // Advance TAA several frames
    ok = run(h, "render.taa.advance", {{"frames","8"}}, msgs);
    EXPECT_TRUE(hasMsg(msgs, "frame=8"));

    // Describe TAA
    EXPECT_TRUE(run(h, "render.taa.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame=8"));
    EXPECT_TRUE(hasMsg(msgs, "jitter=") || hasMsg(msgs, "jitter_x="));

    // Clear and re-validate (empty is valid)
    ok = run(h, "render.graph.clear", {}, msgs);
    EXPECT_TRUE(run(h, "render.graph.validate", {}, msgs));
}

TEST(RenderExtension, StateIsIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    ok = run(h1, "render.graph.record", {{"pass","shadow"}}, msgs);
    ok = run(h1, "render.graph.record", {{"pass","geometry"}}, msgs);

    // h2 should have empty graph
    EXPECT_TRUE(run(h2, "render.graph.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=0"));
}
