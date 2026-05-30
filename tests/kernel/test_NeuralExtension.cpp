// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation neural.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/NeuralExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerNeuralCommands(h);
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

// ── neural.init ───────────────────────────────────────────────────────────────

TEST(NeuralExtension, InitDefaultBackend)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "neural.init", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "neural.init ok"));
    EXPECT_TRUE(hasMsg(msgs, "denoiser="));
    EXPECT_TRUE(hasMsg(msgs, "upscaler="));
}

TEST(NeuralExtension, InitNullBackendDenoiserIsNone)
{
    // On null device no real backends are available:
    // denoiser must be "none", upscaler must be "bilinear"
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "neural.init", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "denoiser=none"));
    EXPECT_TRUE(hasMsg(msgs, "upscaler=bilinear"));
}

TEST(NeuralExtension, InitWithPreferencesDisabled)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "neural.init",
        {{"prefer_dlss","0"},{"prefer_xess","0"},{"prefer_fsr","0"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "denoiser=none"));
    EXPECT_TRUE(hasMsg(msgs, "upscaler=bilinear"));
}

TEST(NeuralExtension, InitCanReinitialise)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "neural.init", {}, msgs);
    EXPECT_TRUE(run(h, "neural.init", {{"prefer_dlss","0"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "neural.init ok"));
}

TEST(NeuralExtension, InitMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "neural.init", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── neural.describe ───────────────────────────────────────────────────────────

TEST(NeuralExtension, DescribeBeforeInitFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "neural.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(NeuralExtension, DescribeAfterInit)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "neural.init", {}, msgs);
    EXPECT_TRUE(run(h, "neural.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "neural.describe"));
    EXPECT_TRUE(hasMsg(msgs, "denoiser=none"));
    EXPECT_TRUE(hasMsg(msgs, "upscaler=bilinear"));
}

TEST(NeuralExtension, DescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "neural.init", {}, msgs);
    EXPECT_TRUE(run(h, "neural.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── Integration ───────────────────────────────────────────────────────────────

TEST(NeuralExtension, FullPipeline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Init with all preferences on (null device → bilinear fallback)
    EXPECT_TRUE(run(h, "neural.init",
        {{"prefer_dlss","1"},{"prefer_xess","1"},{"prefer_fsr","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "denoiser=none"));
    EXPECT_TRUE(hasMsg(msgs, "upscaler=bilinear"));

    // Describe confirms the same state
    EXPECT_TRUE(run(h, "neural.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "denoiser=none"));
    EXPECT_TRUE(hasMsg(msgs, "upscaler=bilinear"));

    // Re-init (replace) succeeds
    EXPECT_TRUE(run(h, "neural.init", {{"prefer_dlss","0"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "neural.init ok"));
}

TEST(NeuralExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h1, "neural.init", {}, msgs);

    // h2 has its own state — describe should fail (not initialised)
    EXPECT_FALSE(run(h2, "neural.describe", {}, msgs));
}
