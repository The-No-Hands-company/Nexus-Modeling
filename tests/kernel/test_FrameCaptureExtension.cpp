// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation capture.* and splat_pass.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/FrameCaptureExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerFrameCaptureCommands(h);
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

static bool runCtx(ScriptBatchHarness& h, const std::string& name,
                   std::initializer_list<std::pair<std::string,std::string>> args,
                   ScriptContext& ctx,
                   std::vector<std::string>& msgs)
{
    ScriptCommand cmd;
    cmd.name = name;
    for (auto& [k, v] : args) cmd.args[k] = v;
    msgs.clear();
    return h.registry().execute(ctx, cmd, msgs);
}

static bool hasMsg(const std::vector<std::string>& msgs, const std::string& sub)
{
    for (const auto& m : msgs)
        if (m.find(sub) != std::string::npos) return true;
    return false;
}

// ── capture.begin ─────────────────────────────────────────────────────────────

TEST(FrameCaptureExtension, BeginDefaultFrame)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "capture.begin", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "capture.begin ok"));
    EXPECT_TRUE(hasMsg(msgs, "frame=0"));
}

TEST(FrameCaptureExtension, BeginExplicitFrame)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "capture.begin", {{"frame","42"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame=42"));
}

// ── capture.record_pass ───────────────────────────────────────────────────────

TEST(FrameCaptureExtension, RecordPassDefault)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "capture.begin", {{"frame","1"}}, msgs);
    EXPECT_TRUE(run(h, "capture.record_pass", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "capture.record_pass ok"));
    EXPECT_TRUE(hasMsg(msgs, "pass=composite"));
    EXPECT_TRUE(hasMsg(msgs, "draw_calls=0"));
}

TEST(FrameCaptureExtension, RecordPassShadow)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "capture.begin", {}, msgs);
    EXPECT_TRUE(run(h, "capture.record_pass",
        {{"pass","shadow"},{"draw_calls","8"},{"triangles","1024"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "pass=shadow"));
    EXPECT_TRUE(hasMsg(msgs, "draw_calls=8"));
    EXPECT_TRUE(hasMsg(msgs, "triangles=1024"));
}

TEST(FrameCaptureExtension, RecordPassGeometry)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "capture.begin", {}, msgs);
    EXPECT_TRUE(run(h, "capture.record_pass",
        {{"pass","geometry"},{"draw_calls","16"},{"triangles","4096"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "pass=geometry"));
}

// ── capture.end ───────────────────────────────────────────────────────────────

TEST(FrameCaptureExtension, EndIncrementsTotalFrames)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "capture.begin", {{"frame","0"}}, msgs);
    ok = run(h, "capture.end",   {}, msgs);
    EXPECT_TRUE(hasMsg(msgs, "total_frames=1"));
    ok = run(h, "capture.begin", {{"frame","1"}}, msgs);
    EXPECT_TRUE(run(h, "capture.end", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "total_frames=2"));
}

// ── capture.describe ──────────────────────────────────────────────────────────

TEST(FrameCaptureExtension, DescribeBeforeSnapshotFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "capture.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(FrameCaptureExtension, DescribeAfterFrame)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "capture.begin",       {{"frame","5"}}, msgs);
    ok = run(h, "capture.record_pass", {{"pass","shadow"},{"draw_calls","4"},{"triangles","512"}}, msgs);
    ok = run(h, "capture.record_pass", {{"pass","geometry"},{"draw_calls","8"},{"triangles","2048"}}, msgs);
    ok = run(h, "capture.end",         {}, msgs);
    EXPECT_TRUE(run(h, "capture.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "frame=5"));
    EXPECT_TRUE(hasMsg(msgs, "passes=2"));
    EXPECT_TRUE(hasMsg(msgs, "draw_calls=12"));
    EXPECT_TRUE(hasMsg(msgs, "triangles=2560"));
}

TEST(FrameCaptureExtension, DescribePerPassLines)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "capture.begin",       {{"frame","1"}}, msgs);
    ok = run(h, "capture.record_pass", {{"pass","shadow"}}, msgs);
    ok = run(h, "capture.end",         {}, msgs);
    EXPECT_TRUE(run(h, "capture.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "index=0"));
    EXPECT_TRUE(hasMsg(msgs, "type=shadow"));
}

// ── capture.snapshot ─────────────────────────────────────────────────────────

TEST(FrameCaptureExtension, SnapshotRequiresCapture)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "capture.snapshot", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(FrameCaptureExtension, SnapshotOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "capture.begin",  {{"frame","3"}}, msgs);
    ok = run(h, "capture.record_pass", {}, msgs);
    ok = run(h, "capture.end",    {}, msgs);
    EXPECT_TRUE(run(h, "capture.snapshot", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "capture.snapshot ok"));
    EXPECT_TRUE(hasMsg(msgs, "frame=3"));
}

// ── capture.diff ─────────────────────────────────────────────────────────────

TEST(FrameCaptureExtension, DiffRequiresBaseline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "capture.diff", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(FrameCaptureExtension, DiffSameFrame)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "capture.begin",    {{"frame","10"}}, msgs);
    ok = run(h, "capture.end",      {}, msgs);
    ok = run(h, "capture.snapshot", {}, msgs);
    EXPECT_TRUE(run(h, "capture.diff", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "delta_frames=0"));
}

TEST(FrameCaptureExtension, DiffDeltaFrames)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    // Baseline at frame 5
    ok = run(h, "capture.begin",    {{"frame","5"}}, msgs);
    ok = run(h, "capture.end",      {}, msgs);
    ok = run(h, "capture.snapshot", {}, msgs);
    // Advance to frame 9
    ok = run(h, "capture.begin", {{"frame","9"}}, msgs);
    ok = run(h, "capture.end",   {}, msgs);
    EXPECT_TRUE(run(h, "capture.diff", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "baseline_frame=5"));
    EXPECT_TRUE(hasMsg(msgs, "current_frame=9"));
    EXPECT_TRUE(hasMsg(msgs, "delta_frames=4"));
}

// ── splat_pass.configure ─────────────────────────────────────────────────────

TEST(FrameCaptureExtension, SplatPassConfigureDefaults)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "splat_pass.configure", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "splat_pass.configure ok"));
    EXPECT_TRUE(hasMsg(msgs, "scale="));
    EXPECT_TRUE(hasMsg(msgs, "sort="));
}

TEST(FrameCaptureExtension, SplatPassConfigureScale)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "splat_pass.configure", {{"scale","2.0"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "scale=2.000"));
}

TEST(FrameCaptureExtension, SplatPassConfigureSortNone)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "splat_pass.configure", {{"sort","none"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "sort=none"));
}

TEST(FrameCaptureExtension, SplatPassConfigureSortCpu)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "splat_pass.configure", {{"sort","none"}}, msgs);
    EXPECT_TRUE(run(h, "splat_pass.configure", {{"sort","cpu"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "sort=cpu"));
}

// ── splat_pass.attach ────────────────────────────────────────────────────────

TEST(FrameCaptureExtension, AttachRequiresCloud)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    ScriptContext ctx;
    EXPECT_FALSE(runCtx(h, "splat_pass.attach", {}, ctx, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(FrameCaptureExtension, AttachSingleCloud)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    ScriptContext ctx;
    // Add 3 splats to the cloud
    ctx.gaussianCloud.splats.resize(3);
    ctx.hasGaussianCloud = true;
    EXPECT_TRUE(runCtx(h, "splat_pass.attach", {}, ctx, msgs));
    EXPECT_TRUE(hasMsg(msgs, "splats=3"));
    EXPECT_TRUE(hasMsg(msgs, "attached=1"));
}

TEST(FrameCaptureExtension, AttachMultipleClouds)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    ScriptContext ctx;
    ctx.gaussianCloud.splats.resize(5);
    ctx.hasGaussianCloud = true;
    ok = runCtx(h, "splat_pass.attach", {}, ctx, msgs);

    ScriptContext ctx2;
    ctx2.gaussianCloud.splats.resize(7);
    ctx2.hasGaussianCloud = true;
    EXPECT_TRUE(runCtx(h, "splat_pass.attach", {}, ctx2, msgs));
    EXPECT_TRUE(hasMsg(msgs, "attached=2"));
}

// ── splat_pass.clear ─────────────────────────────────────────────────────────

TEST(FrameCaptureExtension, ClearClouds)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    ScriptContext ctx;
    ctx.gaussianCloud.splats.resize(4);
    ctx.hasGaussianCloud = true;
    ok = runCtx(h, "splat_pass.attach", {}, ctx, msgs);
    EXPECT_TRUE(run(h, "splat_pass.clear", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "splat_pass.clear ok"));

    // After clear, attached count should be 0
    EXPECT_TRUE(run(h, "splat_pass.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "clouds=0"));
}

// ── splat_pass.compute_stats ─────────────────────────────────────────────────

TEST(FrameCaptureExtension, ComputeStatsEmpty)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "splat_pass.compute_stats", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "splat_pass.compute_stats ok"));
    EXPECT_TRUE(hasMsg(msgs, "draw_calls="));
    EXPECT_TRUE(hasMsg(msgs, "submitted="));
    EXPECT_TRUE(hasMsg(msgs, "projected="));
    EXPECT_TRUE(hasMsg(msgs, "discarded="));
}

TEST(FrameCaptureExtension, ComputeStatsWithCloud)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    ScriptContext ctx;
    ctx.gaussianCloud.splats.resize(10);
    ctx.hasGaussianCloud = true;
    ok = runCtx(h, "splat_pass.attach", {}, ctx, msgs);
    EXPECT_TRUE(run(h, "splat_pass.compute_stats", {}, msgs));
    // submitted should reflect total splats across attached clouds
    EXPECT_TRUE(hasMsg(msgs, "submitted=10"));
}

// ── splat_pass.describe ───────────────────────────────────────────────────────

TEST(FrameCaptureExtension, DescribePassInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "splat_pass.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "splat_pass.describe"));
    EXPECT_TRUE(hasMsg(msgs, "clouds=0"));
    EXPECT_TRUE(hasMsg(msgs, "scale="));
    EXPECT_TRUE(hasMsg(msgs, "sort="));
}

TEST(FrameCaptureExtension, DescribePassAfterConfigure)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "splat_pass.configure",
        {{"scale","1.5"},{"sort","none"}}, msgs);
    EXPECT_TRUE(run(h, "splat_pass.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "scale=1.500"));
    EXPECT_TRUE(hasMsg(msgs, "sort=none"));
}

// ── Integration: full frame capture + splat pass pipeline ────────────────────

TEST(FrameCaptureExtension, FullPipeline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Configure splat pass
    ok = run(h, "splat_pass.configure",
        {{"scale","1.2"},{"sort","cpu"}}, msgs);

    // Attach a cloud with 20 splats
    {
        ScriptContext ctx;
        ctx.gaussianCloud.splats.resize(20);
        ctx.hasGaussianCloud = true;
        ok = runCtx(h, "splat_pass.attach", {}, ctx, msgs);
        EXPECT_TRUE(hasMsg(msgs, "attached=1"));
    }

    // Capture frame 0
    ok = run(h, "capture.begin",       {{"frame","0"}}, msgs);
    ok = run(h, "capture.record_pass", {{"pass","shadow"},  {"draw_calls","4"},{"triangles","512"}},  msgs);
    ok = run(h, "capture.record_pass", {{"pass","geometry"},{"draw_calls","8"},{"triangles","2048"}}, msgs);
    ok = run(h, "capture.record_pass", {{"pass","composite"},{"draw_calls","1"},{"triangles","6"}},   msgs);
    ok = run(h, "capture.end", {}, msgs);

    // Snapshot frame 0 as baseline
    ok = run(h, "capture.snapshot", {}, msgs);
    EXPECT_TRUE(hasMsg(msgs, "frame=0"));

    // Describe — should see 3 passes, totals = 13 draw calls, 2566 triangles
    EXPECT_TRUE(run(h, "capture.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=3"));
    EXPECT_TRUE(hasMsg(msgs, "draw_calls=13"));
    EXPECT_TRUE(hasMsg(msgs, "triangles=2566"));

    // Capture frame 5
    ok = run(h, "capture.begin", {{"frame","5"}}, msgs);
    ok = run(h, "capture.end",   {}, msgs);

    // Diff — delta should be 5
    EXPECT_TRUE(run(h, "capture.diff", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "delta_frames=5"));

    // Splat stats
    EXPECT_TRUE(run(h, "splat_pass.compute_stats", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "submitted=20"));

    // Clear and verify
    ok = run(h, "splat_pass.clear", {}, msgs);
    EXPECT_TRUE(run(h, "splat_pass.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "clouds=0"));
}

TEST(FrameCaptureExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    ok = run(h1, "capture.begin", {{"frame","99"}}, msgs);
    ok = run(h1, "capture.end",   {}, msgs);

    // h2 should have no snapshot
    EXPECT_FALSE(run(h2, "capture.describe", {}, msgs));
}
