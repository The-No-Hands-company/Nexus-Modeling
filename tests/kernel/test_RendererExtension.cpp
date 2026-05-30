// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation renderer.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/RendererExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerRendererCommands(h);
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

// ── renderer.settings.describe ───────────────────────────────────────────────

TEST(RendererExtension, SettingsDescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "mode=Rasterize"));
    EXPECT_TRUE(hasMsg(msgs, "upscale=Off"));
    EXPECT_TRUE(hasMsg(msgs, "taa=1"));
}

TEST(RendererExtension, SettingsDescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── renderer.settings.set ────────────────────────────────────────────────────

TEST(RendererExtension, SettingsSetModeRasterize)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.set", {{"mode","Rasterize"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "mode=Rasterize"));
}

TEST(RendererExtension, SettingsSetModeWireframe)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.set", {{"mode","Wireframe"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "mode=Wireframe"));
}

TEST(RendererExtension, SettingsSetModeNormals)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.set", {{"mode","Normals"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "mode=Normals"));
}

TEST(RendererExtension, SettingsSetModeOverdraw)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.set", {{"mode","Overdraw"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "mode=Overdraw"));
}

TEST(RendererExtension, SettingsSetModePathTraceDowngraded)
{
    // Null backend has no RT — PathTrace should downgrade to Rasterize
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.set", {{"mode","PathTrace"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "mode=Rasterize"));
}

TEST(RendererExtension, SettingsSetUpscaleBilinear)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.set", {{"upscale","Bilinear"}}, msgs));
    EXPECT_TRUE(run(h, "renderer.settings.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "upscale=Bilinear"));
}

TEST(RendererExtension, SettingsSetTaaOff)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.set", {{"taa","0"}}, msgs));
    EXPECT_TRUE(run(h, "renderer.settings.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "taa=0"));
}

TEST(RendererExtension, SettingsSetRenderScale)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.settings.set", {{"scale","0.5"}}, msgs));
    EXPECT_TRUE(run(h, "renderer.settings.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "scale="));
}

// ── renderer.frame.run ───────────────────────────────────────────────────────

TEST(RendererExtension, FrameRunOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.frame.run", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "renderer.frame.run ok"));
}

TEST(RendererExtension, FrameRunMultiple)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "renderer.frame.run", {}, msgs);
    ok = run(h, "renderer.frame.run", {}, msgs);
    EXPECT_TRUE(run(h, "renderer.frame.run", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "renderer.frame.run ok"));
}

// ── renderer.frame_stats.describe ────────────────────────────────────────────

TEST(RendererExtension, FrameStatsDescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.frame_stats.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes="));
    EXPECT_TRUE(hasMsg(msgs, "draws="));
    EXPECT_TRUE(hasMsg(msgs, "gpu_ms="));
}

TEST(RendererExtension, FrameStatsDescribeAfterFrame)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "renderer.frame.run", {}, msgs);
    EXPECT_TRUE(run(h, "renderer.frame_stats.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "nodes=0"));
}

// ── renderer.validation.enable / disable / describe ──────────────────────────

TEST(RendererExtension, ValidationDescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.validation.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "issues="));
}

TEST(RendererExtension, ValidationEnableOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.validation.enable", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "renderer.validation.enable ok"));
    EXPECT_TRUE(run(h, "renderer.validation.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "enabled=1"));
}

TEST(RendererExtension, ValidationDisableOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "renderer.validation.enable", {}, msgs);
    EXPECT_TRUE(run(h, "renderer.validation.disable", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "renderer.validation.disable ok"));
    EXPECT_TRUE(run(h, "renderer.validation.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "enabled=0"));
}

TEST(RendererExtension, ValidationDescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.validation.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── renderer.resize ──────────────────────────────────────────────────────────

TEST(RendererExtension, ResizeOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "renderer.resize", {{"width","1920"},{"height","1080"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "renderer.resize ok"));
    EXPECT_TRUE(hasMsg(msgs, "width=1920"));
    EXPECT_TRUE(hasMsg(msgs, "height=1080"));
}

// ── Integration ───────────────────────────────────────────────────────────────

TEST(RendererExtension, FullPipeline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Set wireframe mode
    ok = run(h, "renderer.settings.set", {{"mode","Wireframe"}}, msgs);
    EXPECT_TRUE(run(h, "renderer.settings.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "mode=Wireframe"));

    // Run a frame
    EXPECT_TRUE(run(h, "renderer.frame.run", {}, msgs));

    // Check stats
    EXPECT_TRUE(run(h, "renderer.frame_stats.describe", {}, msgs));

    // Enable validation, run another frame, check report
    ok = run(h, "renderer.validation.enable", {}, msgs);
    ok = run(h, "renderer.frame.run", {}, msgs);
    EXPECT_TRUE(run(h, "renderer.validation.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "enabled=1"));

    // Resize
    EXPECT_TRUE(run(h, "renderer.resize", {{"width","2560"},{"height","1440"}}, msgs));

    // Disable validation
    ok = run(h, "renderer.validation.disable", {}, msgs);
    EXPECT_TRUE(run(h, "renderer.validation.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "enabled=0"));
}

TEST(RendererExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h1, "renderer.settings.set",
        {{"mode","Wireframe"}}, msgs);

    EXPECT_TRUE(run(h2, "renderer.settings.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "mode=Rasterize"));
}
