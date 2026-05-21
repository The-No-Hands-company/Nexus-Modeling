#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>

#include <nexus/asset/SceneAsset.h>

#include <gtest/gtest.h>

namespace {

// Stateless deleter for std::unique_ptr<FILE, ...> over popen()/pclose().
// Avoids -Wignored-attributes warning produced when decltype(&pclose) strips
// the libc nothrow attribute during template-argument deduction.
struct PCloseDeleter {
    void operator()(FILE* file) const noexcept {
        if (file != nullptr) {
            pclose(file);
        }
    }
};

std::string runCommand(const std::string& command)
{
    std::unique_ptr<FILE, PCloseDeleter> pipe(popen(command.c_str(), "r"));
    if (!pipe) {
        return "(popen failed)";
    }

    std::ostringstream result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result << buffer;
    }

    return result.str();
}

int runCommandReturnCode(const std::string& command)
{
    return WEXITSTATUS(system(command.c_str()));
}

} // namespace

class ToolingAppSmokeTests : public ::testing::Test {
protected:
#ifdef NEXUS_TOOLING_APP_PATH
    std::string appPath{NEXUS_TOOLING_APP_PATH};
#else
    std::string appPath{"./build/src/tooling/nexus_tooling_app"};
#endif
};

TEST_F(ToolingAppSmokeTests, HelpOutputNotEmpty)
{
    std::string output = runCommand(appPath + " --help");
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Usage"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, HelpExitsCleanly)
{
    int exitCode = runCommandReturnCode(appPath + " --help > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ListScenariosWithDefaultManifest)
{
    std::string output = runCommand(appPath + " --list-scenarios 2>&1");
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Available scenarios"), std::string::npos);
    EXPECT_NE(output.find("render_scheduler_baseline"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ListScenariosExitsCleanly)
{
    int exitCode = runCommandReturnCode(appPath + " --list-scenarios > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ListScenariosWithMissingManifestReturnsNonZero)
{
    int exitCode =
        runCommandReturnCode(appPath + " --list-scenarios --manifest /tmp/does-not-exist-manifest.tsv > /dev/null 2>&1");
    EXPECT_NE(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ListScenariosWithMissingManifestProducesError)
{
    std::string output =
        runCommand(appPath + " --list-scenarios --manifest /tmp/does-not-exist-manifest.tsv 2>&1");
    EXPECT_NE(output.find("Failed to open"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShowDashboardWithExistingFile)
{
    std::string output = runCommand(appPath + " --show-dashboard 2>&1");
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Capability dashboard"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShowDashboardWithExistingFileExitsCleanly)
{
    int exitCode = runCommandReturnCode(appPath + " --show-dashboard > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ShowDashboardWithMissingFileExitsCleanly)
{
    int exitCode = runCommandReturnCode(appPath +
                                        " --show-dashboard --dashboard /tmp/does-not-exist-dashboard.json > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ShowDashboardWithMissingFileProducesEmptyState)
{
    std::string output =
        runCommand(appPath + " --show-dashboard --dashboard /tmp/does-not-exist-dashboard.json 2>&1");
    EXPECT_NE(output.find("Capability dashboard not found"), std::string::npos);
    EXPECT_NE(output.find("Empty-state"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, UnknownArgumentReturnsNonZero)
{
    int exitCode = runCommandReturnCode(appPath + " --unknown-flag > /dev/null 2>&1");
    EXPECT_NE(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ShowDashboardProducesSubsystemSummary)
{
    std::string output = runCommand(appPath + " --show-dashboard 2>&1");
    EXPECT_NE(output.find("subsystems"), std::string::npos);
    EXPECT_NE(output.find("Graphics Backend"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, AppNoOpModeExitsCleanly)
{
    int exitCode = runCommandReturnCode(appPath + " > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutRendersAllFivePanes)
{
    std::string output = runCommand(appPath + " --shell 2>&1");
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Viewer Shell"), std::string::npos);
    EXPECT_NE(output.find("[1] Scenario List"), std::string::npos);
    EXPECT_NE(output.find("[2] Viewport / Image Panel"), std::string::npos);
    EXPECT_NE(output.find("[3] Diagnostics Panel"), std::string::npos);
    EXPECT_NE(output.find("[4] Subsystem Status Panel"), std::string::npos);
    EXPECT_NE(output.find("[5] Run Controls"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutExitsCleanly)
{
    int exitCode = runCommandReturnCode(appPath + " --shell > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutPopulatesScenarioList)
{
    std::string output = runCommand(appPath + " --shell 2>&1");
    EXPECT_NE(output.find("render_scheduler_baseline"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutHandlesMissingDashboard)
{
    std::string output = runCommand(appPath +
                                    " --shell --dashboard /tmp/does-not-exist-dashboard.json 2>&1");
    EXPECT_NE(output.find("dashboard not found"), std::string::npos);
    EXPECT_NE(output.find("[4] Subsystem Status Panel"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutHandlesMissingManifest)
{
    std::string output = runCommand(appPath +
                                    " --shell --manifest /tmp/does-not-exist-manifest.tsv 2>&1");
    // Still renders all panes even when manifest is absent; only pane 1 shows error.
    EXPECT_NE(output.find("[1] Scenario List"), std::string::npos);
    EXPECT_NE(output.find("Failed to open"), std::string::npos);
    EXPECT_NE(output.find("[5] Run Controls"), std::string::npos);
}

// --- Week 2 Day 2: artifact loader and error states ------------------------

TEST_F(ToolingAppSmokeTests, ShowArtifactsReportsPassStatus)
{
    std::string output = runCommand(appPath + " --show-artifacts render_scheduler_baseline 2>&1");
    EXPECT_NE(output.find("Scenario artifacts: render_scheduler_baseline"), std::string::npos);
    EXPECT_NE(output.find("summary.json: status=pass"), std::string::npos);
    EXPECT_NE(output.find("diagnostics.txt:"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShowArtifactsPassScenarioExitsCleanly)
{
    int exitCode = runCommandReturnCode(appPath + " --show-artifacts render_scheduler_baseline > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ShowArtifactsShowsFrameHashWhenNoPng)
{
    std::string output = runCommand(appPath + " --show-artifacts render_scheduler_baseline 2>&1");
    EXPECT_NE(output.find("frame_hash.txt:"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShowArtifactsShowsEmptyFrameStateWhenNoneProduced)
{
    std::string output = runCommand(appPath + " --show-artifacts asset_automation_pipeline_scene 2>&1");
    EXPECT_NE(output.find("frame.png / frame_hash.txt: missing"), std::string::npos);
    EXPECT_NE(output.find("empty-state"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShowArtifactsMissingDirectoryProducesHint)
{
    std::string output = runCommand(appPath +
                                    " --show-artifacts not_a_real_scenario --artifacts-root /tmp/does-not-exist-artifacts 2>&1");
    EXPECT_NE(output.find("artifacts directory not found"), std::string::npos);
    EXPECT_NE(output.find("hint: run tools/run_scenario.sh"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShowArtifactsMissingDirectoryExitsCleanly)
{
    int exitCode = runCommandReturnCode(appPath +
                                        " --show-artifacts nope --artifacts-root /tmp/does-not-exist-artifacts > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ShowArtifactsRequiresScenarioId)
{
    int exitCode = runCommandReturnCode(appPath + " --show-artifacts > /dev/null 2>&1");
    EXPECT_NE(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ShowArtifactsFailStatusExitsNonZero)
{
    // Synthesize a fail-state artifact tree in a temp dir and verify exit code 1.
    namespace fs = std::filesystem;
    fs::path tempRoot = fs::temp_directory_path() / "nexus_tooling_app_fail_test";
    fs::remove_all(tempRoot);
    fs::create_directories(tempRoot / "synthetic_fail_scenario");
    {
        std::ofstream out(tempRoot / "synthetic_fail_scenario" / "summary.json");
        out << "{\n  \"scenario_id\": \"synthetic_fail_scenario\",\n"
            << "  \"backend\": \"null\",\n  \"status\": \"fail\",\n"
            << "  \"test_filter\": \"X\",\n  \"artifact_version\": 1\n}\n";
    }

    const std::string cmd = appPath + " --show-artifacts synthetic_fail_scenario --artifacts-root "
                            + tempRoot.string() + " 2>&1";
    std::string output = runCommand(cmd);
    int exitCode = runCommandReturnCode(cmd + " > /dev/null");

    EXPECT_NE(output.find("status=fail"), std::string::npos);
    EXPECT_NE(output.find("SCENARIO STATUS: FAIL"), std::string::npos);
    EXPECT_EQ(exitCode, 1);

    fs::remove_all(tempRoot);
}

TEST_F(ToolingAppSmokeTests, ShellWithScenarioPopulatesViewportAndDiagnostics)
{
    std::string output = runCommand(appPath + " --shell --scenario render_scheduler_baseline 2>&1");
    EXPECT_NE(output.find("[2] Viewport / Image Panel"), std::string::npos);
    EXPECT_NE(output.find("frame_hash:"), std::string::npos);
    EXPECT_NE(output.find("[3] Diagnostics Panel"), std::string::npos);
    EXPECT_NE(output.find("lines,"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, ShellWithScenarioHandlesMissingArtifactsRoot)
{
    std::string output = runCommand(appPath +
                                    " --shell --scenario nope --artifacts-root /tmp/does-not-exist-artifacts 2>&1");
    EXPECT_NE(output.find("artifacts directory missing"), std::string::npos);
    EXPECT_NE(output.find("[2] Viewport / Image Panel"), std::string::npos);
    EXPECT_NE(output.find("[3] Diagnostics Panel"), std::string::npos);
}

// --- Week 2 Day 3: run scenario integration --------------------------------

TEST_F(ToolingAppSmokeTests, RunScenarioRequiresScenarioId)
{
    int exitCode = runCommandReturnCode(appPath + " --run-scenario > /dev/null 2>&1");
    EXPECT_NE(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, RunScenarioRejectsUnsafeScenarioId)
{
    int exitCode = runCommandReturnCode(appPath + " --run-scenario 'bad;rm -rf /' > /dev/null 2>&1");
    EXPECT_NE(exitCode, 0);
    std::string output = runCommand(appPath + " --run-scenario 'bad;rm -rf /' 2>&1");
    EXPECT_NE(output.find("unsafe characters"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, RunScenarioRejectsMissingRunner)
{
    int exitCode = runCommandReturnCode(appPath +
                                        " --run-scenario render_scheduler_baseline --runner /tmp/does-not-exist-runner.sh > /dev/null 2>&1");
    EXPECT_NE(exitCode, 0);
    std::string output = runCommand(appPath +
                                    " --run-scenario render_scheduler_baseline --runner /tmp/does-not-exist-runner.sh 2>&1");
    EXPECT_NE(output.find("Scenario runner not found"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, RunScenarioRendersRefreshedArtifactsOnSuccess)
{
    // Drive a real canonical scenario end to end through the app.
    std::string output = runCommand(appPath + " --run-scenario render_scheduler_baseline 2>&1");
    EXPECT_NE(output.find("Running scenario via"), std::string::npos);
    EXPECT_NE(output.find("Scenario runner exit code: 0"), std::string::npos);
    EXPECT_NE(output.find("Refreshed artifacts:"), std::string::npos);
    EXPECT_NE(output.find("summary.json: status=pass"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, RunScenarioExitsZeroOnSuccess)
{
    int exitCode = runCommandReturnCode(appPath +
                                        " --run-scenario render_scheduler_baseline > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, RunScenarioRefreshesParametricRectangleReplay)
{
    std::string output = runCommand(appPath + " --run-scenario parametric_rectangle_replay 2>&1");
    EXPECT_NE(output.find("Refreshed artifacts:"), std::string::npos);
    EXPECT_NE(output.find("Scenario artifacts: parametric_rectangle_replay"), std::string::npos);
    EXPECT_NE(output.find("summary.json: status=pass"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, RunScenarioRefreshesEvalGraphDeterministicEvaluation)
{
    std::string output = runCommand(appPath + " --run-scenario eval_graph_deterministic_evaluation 2>&1");
    EXPECT_NE(output.find("Refreshed artifacts:"), std::string::npos);
    EXPECT_NE(output.find("Scenario artifacts: eval_graph_deterministic_evaluation"), std::string::npos);
    EXPECT_NE(output.find("summary.json: status=pass"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, RunScenarioFailsClearlyOnUnknownScenario)
{
    int exitCode = runCommandReturnCode(appPath + " --run-scenario not_a_real_scenario > /dev/null 2>&1");
    EXPECT_NE(exitCode, 0);
    std::string output = runCommand(appPath + " --run-scenario not_a_real_scenario 2>&1");
    // Underlying runner reports unknown scenario id; we forward and surface non-zero exit.
    EXPECT_NE(output.find("Scenario runner exit code:"), std::string::npos);
}

// --- Week 2 Day 4: dashboard refresh and known gaps ------------------------

namespace {

std::filesystem::path makeTempDir(const std::string& tag)
{
    auto base = std::filesystem::temp_directory_path()
                / ("nexus_tooling_pr9_" + tag + "_"
                   + std::to_string(::getpid()) + "_"
                   + std::to_string(std::rand()));
    std::filesystem::create_directories(base);
    return base;
}

void writeFile(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << contents;
}

} // namespace

TEST_F(ToolingAppSmokeTests, ShowDashboardSurfacesKnownGapsWhenPresent)
{
    auto tmp = makeTempDir("show_dash_gaps");
    writeFile(tmp / "cap.json",
              "{\n  \"generated_at_utc\": \"2026-05-20T00:00:00Z\",\n"
              "  \"subsystems\": [\n"
              "    {\n      \"name\": \"X\",\n      \"status\": \"green\"\n    }\n"
              "  ]\n}\n");
    writeFile(tmp / "gaps.json",
              "[\n  {\"id\": \"P0-DEMO\", \"domain\": \"Demo\", \"severity\": \"S1\","
              " \"status\": \"open\", \"user_impact\": \"demo impact line\"}\n]\n");
    std::string out = runCommand(appPath +
                                 " --show-dashboard --dashboard " + (tmp / "cap.json").string() +
                                 " --known-gaps " + (tmp / "gaps.json").string() + " 2>&1");
    EXPECT_NE(out.find("subsystems:"), std::string::npos);
    EXPECT_NE(out.find("Known gaps:"), std::string::npos);
    EXPECT_NE(out.find("P0-DEMO"), std::string::npos);
    EXPECT_NE(out.find("demo impact line"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ShowDashboardReportsCleanStateWhenGapsArrayEmpty)
{
    auto tmp = makeTempDir("show_dash_clean");
    writeFile(tmp / "cap.json",
              "{\n  \"subsystems\": [\n"
              "    {\n      \"name\": \"X\",\n      \"status\": \"green\"\n    }\n"
              "  ]\n}\n");
    writeFile(tmp / "gaps.json", "[]\n");
    std::string out = runCommand(appPath +
                                 " --show-dashboard --dashboard " + (tmp / "cap.json").string() +
                                 " --known-gaps " + (tmp / "gaps.json").string() + " 2>&1");
    EXPECT_NE(out.find("Known gaps:"), std::string::npos);
    EXPECT_NE(out.find("no open gaps"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ShowDashboardKnownGapsMissingFileEmptyState)
{
    auto tmp = makeTempDir("show_dash_missing_gaps");
    writeFile(tmp / "cap.json", "{}\n");
    std::string out = runCommand(appPath +
                                 " --show-dashboard --dashboard " + (tmp / "cap.json").string() +
                                 " --known-gaps " + (tmp / "no_such_gaps.json").string() + " 2>&1");
    EXPECT_NE(out.find("Known gaps:"), std::string::npos);
    EXPECT_NE(out.find("file not found"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutIncludesKnownGapsPane)
{
    auto tmp = makeTempDir("shell_gaps");
    writeFile(tmp / "gaps.json",
              "[\n  {\"id\": \"P0-FOO\", \"domain\": \"Bar\", \"severity\": \"S2\","
              " \"status\": \"open\", \"user_impact\": \"x\"}\n]\n");
    std::string out = runCommand(appPath + " --shell --known-gaps "
                                 + (tmp / "gaps.json").string() + " 2>&1");
    EXPECT_NE(out.find("[4b] Known Gaps"), std::string::npos);
    EXPECT_NE(out.find("P0-FOO"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutKnownGapsMissingShowsEmptyState)
{
    std::string out = runCommand(appPath + " --shell --known-gaps /tmp/does-not-exist-gaps-xyz.json 2>&1");
    EXPECT_NE(out.find("[4b] Known Gaps"), std::string::npos);
    EXPECT_NE(out.find("known_gaps.json not found"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, RefreshDashboardRejectsMissingScript)
{
    int exitCode = runCommandReturnCode(appPath +
                                        " --refresh-dashboard --dashboard-script /tmp/no-such-dashboard.sh > /dev/null 2>&1");
    EXPECT_NE(exitCode, 0);
    std::string out = runCommand(appPath +
                                 " --refresh-dashboard --dashboard-script /tmp/no-such-dashboard.sh 2>&1");
    EXPECT_NE(out.find("Dashboard script not found"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, RefreshDashboardSucceedsAndRefreshesView)
{
    auto tmp = makeTempDir("refresh_ok");
    const auto cap = tmp / "cap.json";
    const auto gaps = tmp / "gaps.json";
    int exitCode = runCommandReturnCode(appPath +
                                        " --refresh-dashboard --skip-gates --dashboard " + cap.string() +
                                        " --known-gaps " + gaps.string() + " > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(std::filesystem::exists(cap));
    EXPECT_TRUE(std::filesystem::exists(gaps));

    std::string out = runCommand(appPath +
                                 " --refresh-dashboard --skip-gates --dashboard " + cap.string() +
                                 " --known-gaps " + gaps.string() + " 2>&1");
    EXPECT_NE(out.find("Refreshing dashboard via"), std::string::npos);
    EXPECT_NE(out.find("Dashboard script exit code: 0"), std::string::npos);
    EXPECT_NE(out.find("Refreshed dashboard:"), std::string::npos);
    EXPECT_NE(out.find("subsystems:"), std::string::npos);
    EXPECT_NE(out.find("Known gaps:"), std::string::npos);

    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, RefreshDashboardForwardsScriptExitCode)
{
    // Drive the dashboard script via a stub that prints output and exits 7,
    // proving the app surfaces failure cleanly instead of crashing.
    auto tmp = makeTempDir("refresh_fail");
    const auto stub = tmp / "stub.sh";
    {
        std::ofstream out(stub);
        out << "#!/usr/bin/env bash\n"
               "echo 'stub: simulating failure'\n"
               "exit 7\n";
    }
    std::filesystem::permissions(stub,
                                 std::filesystem::perms::owner_all
                                     | std::filesystem::perms::group_read
                                     | std::filesystem::perms::group_exec
                                     | std::filesystem::perms::others_read
                                     | std::filesystem::perms::others_exec);

    int exitCode = runCommandReturnCode(appPath +
                                        " --refresh-dashboard --dashboard-script " + stub.string() +
                                        " --dashboard " + (tmp / "cap.json").string() +
                                        " --known-gaps " + (tmp / "gaps.json").string() +
                                        " > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 7);

    std::string out = runCommand(appPath +
                                 " --refresh-dashboard --dashboard-script " + stub.string() +
                                 " --dashboard " + (tmp / "cap.json").string() +
                                 " --known-gaps " + (tmp / "gaps.json").string() + " 2>&1");
    EXPECT_NE(out.find("stub: simulating failure"), std::string::npos);
    EXPECT_NE(out.find("Dashboard script exit code: 7"), std::string::npos);
    EXPECT_NE(out.find("file not written"), std::string::npos);

    std::filesystem::remove_all(tmp);
}

// --- Week 2 Day 5: smoke coverage expansion --------------------------------

TEST_F(ToolingAppSmokeTests, ShowDashboardMalformedJsonShowsUnrecognizedFormat)
{
    auto tmp = makeTempDir("dash_malformed");
    writeFile(tmp / "cap.json", "this is not json {{{ broken");
    std::string out = runCommand(appPath + " --show-dashboard --dashboard "
                                 + (tmp / "cap.json").string() + " 2>&1");
    EXPECT_NE(out.find("subsystem summary: unavailable"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ShowDashboardMalformedJsonExitsCleanly)
{
    auto tmp = makeTempDir("dash_malformed_exit");
    writeFile(tmp / "cap.json", "this is not json {{{ broken");
    int exitCode = runCommandReturnCode(appPath + " --show-dashboard --dashboard "
                                        + (tmp / "cap.json").string() + " > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutTolerantOfMalformedDashboard)
{
    auto tmp = makeTempDir("shell_malformed_dash");
    writeFile(tmp / "cap.json", "not-json}}}");
    std::string out = runCommand(appPath + " --shell --dashboard "
                                 + (tmp / "cap.json").string() + " 2>&1");
    // Layout still rendered end-to-end; pane 4 surfaces unrecognized-format note.
    EXPECT_NE(out.find("[4] Subsystem Status Panel"), std::string::npos);
    EXPECT_NE(out.find("dashboard format unrecognized"), std::string::npos);
    EXPECT_NE(out.find("[5] Run Controls"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, KnownGapsMalformedJsonDoesNotCrashShowDashboard)
{
    auto tmp = makeTempDir("gaps_malformed");
    writeFile(tmp / "cap.json", "{}\n");
    writeFile(tmp / "gaps.json", "totally bogus, no braces here at all");
    int exitCode = runCommandReturnCode(appPath +
                                        " --show-dashboard --dashboard " + (tmp / "cap.json").string() +
                                        " --known-gaps " + (tmp / "gaps.json").string() +
                                        " > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
    std::string out = runCommand(appPath +
                                 " --show-dashboard --dashboard " + (tmp / "cap.json").string() +
                                 " --known-gaps " + (tmp / "gaps.json").string() + " 2>&1");
    EXPECT_NE(out.find("Known gaps:"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, KnownGapsTruncatedJsonDoesNotCrashShell)
{
    auto tmp = makeTempDir("gaps_truncated");
    // Open brace with no close - parser must terminate without infinite loop.
    writeFile(tmp / "gaps.json", "[ { \"id\": \"P0-X\", ");
    int exitCode = runCommandReturnCode(appPath + " --shell --known-gaps "
                                        + (tmp / "gaps.json").string() + " > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ListScenariosEmptyManifestSurfacesEmptyState)
{
    auto tmp = makeTempDir("manifest_empty");
    writeFile(tmp / "manifest.tsv", "");
    std::string out = runCommand(appPath + " --list-scenarios --manifest "
                                 + (tmp / "manifest.tsv").string() + " 2>&1");
    EXPECT_NE(out.find("Available scenarios:"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ListScenariosSkipsCommentsAndBlankLines)
{
    auto tmp = makeTempDir("manifest_skip");
    writeFile(tmp / "manifest.tsv",
              "# header comment line\n"
              "\n"
              "good_one\tTitle\tnull\tscripts/scenarios/good_one.sh\ta good scenario\n"
              "\n"
              "# another comment\n"
              "good_two\tTitle2\tnull\tscripts/scenarios/good_two.sh\tanother scenario\n");
    std::string out = runCommand(appPath + " --list-scenarios --manifest "
                                 + (tmp / "manifest.tsv").string() + " 2>&1");
    EXPECT_NE(out.find("good_one"), std::string::npos);
    EXPECT_NE(out.find("good_two"), std::string::npos);
    EXPECT_EQ(out.find("comment"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ListScenariosShortRowSurfacesParseError)
{
    auto tmp = makeTempDir("manifest_short");
    writeFile(tmp / "manifest.tsv",
              "good_one\tTitle\tnull\tscripts/scenarios/good_one.sh\ta good scenario\n"
              "broken\trow\ttoo_short\n");
    int exitCode = runCommandReturnCode(appPath + " --list-scenarios --manifest "
                                        + (tmp / "manifest.tsv").string() + " > /dev/null 2>&1");
    EXPECT_NE(exitCode, 0);
    std::string out = runCommand(appPath + " --list-scenarios --manifest "
                                 + (tmp / "manifest.tsv").string() + " 2>&1");
    EXPECT_NE(out.find("Malformed manifest line"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutWithEmptyManifestRendersEmptyState)
{
    auto tmp = makeTempDir("shell_empty_manifest");
    writeFile(tmp / "manifest.tsv", "");
    std::string out = runCommand(appPath + " --shell --manifest "
                                 + (tmp / "manifest.tsv").string() + " 2>&1");
    EXPECT_NE(out.find("[1] Scenario List"), std::string::npos);
    EXPECT_NE(out.find("(manifest empty)"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, RunScenarioRefreshReflectsLatestArtifactsContent)
{
    // Prove the post-run refresh re-reads artifacts from disk (not a cached view)
    // by driving --run-scenario with a stub runner that writes specific summary content,
    // then running again with different content via the same redirected artifacts root.
    auto tmp = makeTempDir("refresh_proof");
    const auto artifactsRoot = tmp / "artifacts";
    std::filesystem::create_directories(artifactsRoot);

    const auto stub = tmp / "stub_runner.sh";
    {
        std::ofstream out(stub);
        // Runner contract: <scenario_id> [backend]. Writes summary.json and diagnostics.txt
        // into <artifactsRoot>/<id>/, reading the desired status from $REFRESH_STUB_STATUS.
        out << "#!/usr/bin/env bash\n"
               "set -euo pipefail\n"
               "scenario_id=\"$1\"\n"
               "dir=\"" << artifactsRoot.string() << "/${scenario_id}\"\n"
               "mkdir -p \"$dir\"\n"
               "status=\"${REFRESH_STUB_STATUS:-pass}\"\n"
               "marker=\"${REFRESH_STUB_MARKER:-default-marker}\"\n"
               "cat >\"$dir/summary.json\" <<JSON\n"
               "{\"status\": \"${status}\", \"backend\": \"null\", \"test_filter\": \"${marker}\"}\n"
               "JSON\n"
               "echo \"stub runner produced ${status} for ${marker}\" >\"$dir/diagnostics.txt\"\n";
    }
    std::filesystem::permissions(stub,
                                 std::filesystem::perms::owner_all
                                     | std::filesystem::perms::group_read
                                     | std::filesystem::perms::group_exec
                                     | std::filesystem::perms::others_read
                                     | std::filesystem::perms::others_exec);

    // First run: pass + marker_one.
    const std::string base = appPath +
                             " --run-scenario refresh_proof_scenario --runner " + stub.string() +
                             " --artifacts-root " + artifactsRoot.string();
    std::string out1 = runCommand("REFRESH_STUB_STATUS=pass REFRESH_STUB_MARKER=marker_one "
                                  + base + " 2>&1");
    EXPECT_NE(out1.find("Refreshed artifacts:"), std::string::npos);
    EXPECT_NE(out1.find("status=pass"), std::string::npos);
    EXPECT_NE(out1.find("marker_one"), std::string::npos);

    // Second run: fail + marker_two. Refreshed view must show new content + non-zero exit.
    int exitCode2 = runCommandReturnCode("REFRESH_STUB_STATUS=fail REFRESH_STUB_MARKER=marker_two "
                                         + base + " > /dev/null 2>&1");
    EXPECT_EQ(exitCode2, 1);  // pass-through exit when refreshed summary status is fail.
    std::string out2 = runCommand("REFRESH_STUB_STATUS=fail REFRESH_STUB_MARKER=marker_two "
                                  + base + " 2>&1");
    EXPECT_NE(out2.find("status=fail"), std::string::npos);
    EXPECT_NE(out2.find("marker_two"), std::string::npos);
    EXPECT_EQ(out2.find("marker_one"), std::string::npos);

    std::filesystem::remove_all(tmp);
}

// --- Week 3 Day 1: starter modeling workspace shell (PR-11) ----------------

TEST_F(ToolingAppSmokeTests, StarterWorkspacePrintsHeader)
{
    std::string out = runCommand(appPath + " --starter-workspace 2>&1");
    EXPECT_NE(out.find("Starter modeling workspace"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterWorkspaceListsGuidedIntroSteps)
{
    std::string out = runCommand(appPath + " --starter-workspace 2>&1");
    EXPECT_NE(out.find("Guided intro:"), std::string::npos);
    // From ModelingShell::guidedIntroSteps() public contract:
    EXPECT_NE(out.find("Pick a starter primitive"), std::string::npos);
    EXPECT_NE(out.find("quick cleanup"), std::string::npos);
    EXPECT_NE(out.find("diagnostics overlay"), std::string::npos);
    EXPECT_NE(out.find("Export"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterWorkspaceListsAllPrimitives)
{
    std::string out = runCommand(appPath + " --starter-workspace 2>&1");
    EXPECT_NE(out.find("Available starter primitives:"), std::string::npos);
    EXPECT_NE(out.find("Box"), std::string::npos);
    EXPECT_NE(out.find("Plane"), std::string::npos);
    EXPECT_NE(out.find("Sphere"), std::string::npos);
    EXPECT_NE(out.find("Cylinder"), std::string::npos);
    EXPECT_NE(out.find("Cone"), std::string::npos);
    EXPECT_NE(out.find("Capsule"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterWorkspaceOmitsAdvancedPanels)
{
    // Simple-by-default: advanced authoring knobs must not appear in Day-1 surface.
    std::string out = runCommand(appPath + " --starter-workspace 2>&1");
    EXPECT_EQ(out.find("BevelChamferDesc"), std::string::npos);
    EXPECT_EQ(out.find("RemeshDesc"), std::string::npos);
    EXPECT_EQ(out.find("MeshDiagnosticOverlayOptions"), std::string::npos);
    EXPECT_EQ(out.find("HardSurfaceWorkflow"), std::string::npos);
    EXPECT_NE(out.find("hidden by design"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterWorkspaceExitsZero)
{
    int exitCode = runCommandReturnCode(appPath + " --starter-workspace > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, ShellLayoutIncludesStarterWorkspacePane)
{
    std::string out = runCommand(appPath + " --shell 2>&1");
    EXPECT_NE(out.find("[6] Starter Workspace"), std::string::npos);
    EXPECT_NE(out.find("Box, Plane, Sphere, Cylinder, Cone, Capsule"), std::string::npos);
    EXPECT_NE(out.find("--starter-workspace"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, HelpListsStarterWorkspaceFlag)
{
    std::string out = runCommand(appPath + " --help 2>&1");
    EXPECT_NE(out.find("--starter-workspace"), std::string::npos);
}

// --- Week 3 Day 2: starter-model flow integration (PR-12) ------------------

TEST_F(ToolingAppSmokeTests, StarterModelBoxSucceedsAndPrintsSessionReport)
{
    std::string out = runCommand(appPath + " --starter-model Box 2>&1");
    EXPECT_NE(out.find("Starter model session"), std::string::npos);
    EXPECT_NE(out.find("primitive: Box"), std::string::npos);
    EXPECT_NE(out.find("ready: true"), std::string::npos);
    EXPECT_NE(out.find("session.success: true"), std::string::npos);
    EXPECT_NE(out.find("workflow steps"), std::string::npos);
    // The kernel pipeline always emits an Init + RebuildNormals pair for starter primitives.
    EXPECT_NE(out.find("- Init:"), std::string::npos);
    EXPECT_NE(out.find("- RebuildNormals:"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterModelExitsZeroOnSuccess)
{
    int exitCode = runCommandReturnCode(appPath + " --starter-model Sphere > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, StarterModelSurfacesDiagnosticsOverlayReadout)
{
    std::string out = runCommand(appPath + " --starter-model Box 2>&1");
    EXPECT_NE(out.find("diagnostics overlay"), std::string::npos);
    EXPECT_NE(out.find("non_manifold_edges:"), std::string::npos);
    EXPECT_NE(out.find("boundary_edges:"), std::string::npos);
    EXPECT_NE(out.find("degenerate_faces:"), std::string::npos);
    EXPECT_NE(out.find("isolated_vertices:"), std::string::npos);
    // Clean starter primitive should report zero pathological counts.
    EXPECT_NE(out.find("non_manifold_edges: 0"), std::string::npos);
    EXPECT_NE(out.find("degenerate_faces: 0"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterModelAcceptsCaseInsensitivePrimitive)
{
    std::string out = runCommand(appPath + " --starter-model cylinder 2>&1");
    EXPECT_NE(out.find("primitive: cylinder"), std::string::npos);
    EXPECT_NE(out.find("session.success: true"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterModelHonorsSizeArgument)
{
    std::string out = runCommand(appPath + " --starter-model Box --size 2.5 2>&1");
    EXPECT_NE(out.find("size: 2.5"), std::string::npos);
    EXPECT_NE(out.find("session.success: true"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterModelEachPrimitiveSucceeds)
{
    for (const char* name : {"Box", "Plane", "Sphere", "Cylinder", "Cone", "Capsule"}) {
        std::string out = runCommand(appPath + " --starter-model " + name + " 2>&1");
        EXPECT_NE(out.find("session.success: true"), std::string::npos) << "primitive=" << name;
        EXPECT_NE(out.find("ready: true"), std::string::npos) << "primitive=" << name;
    }
}

TEST_F(ToolingAppSmokeTests, StarterModelRejectsUnknownPrimitive)
{
    int exitCode = runCommandReturnCode(appPath + " --starter-model torus > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 2);
    std::string out = runCommand(appPath + " --starter-model torus 2>&1");
    EXPECT_NE(out.find("Unknown starter primitive"), std::string::npos);
    EXPECT_NE(out.find("Box, Plane, Sphere, Cylinder, Cone, Capsule"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterModelMissingArgumentReturnsUsageError)
{
    int exitCode = runCommandReturnCode(appPath + " --starter-model > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 2);
}

TEST_F(ToolingAppSmokeTests, StarterModelRejectsInvalidSize)
{
    int exitCode = runCommandReturnCode(appPath + " --starter-model Box --size not-a-number > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 2);
}

TEST_F(ToolingAppSmokeTests, HelpListsStarterModelFlag)
{
    std::string out = runCommand(appPath + " --help 2>&1");
    EXPECT_NE(out.find("--starter-model"), std::string::npos);
}

// --- Week 3 Day 3: ModelingShell cleanup + diagnostics refresh (PR-13) -----

TEST_F(ToolingAppSmokeTests, StarterModelCleanupAddsWorkflowSteps)
{
    // Baseline: 2 workflow steps (Init + RebuildNormals).
    std::string baseline = runCommand(appPath + " --starter-model Sphere 2>&1");
    EXPECT_NE(baseline.find("workflow steps (2)"), std::string::npos);

    // With --cleanup: Triangulate + BevelChamfer + Triangulate + Remesh +
    // final RebuildNormals append (CG-1 PR-22: quickCleanup pre-triangulates
    // AND re-triangulates after bevel so Remesh always sees triangle input).
    std::string out = runCommand(appPath + " --starter-model Sphere --cleanup 2>&1");
    EXPECT_NE(out.find("workflow steps (7)"), std::string::npos);
    EXPECT_NE(out.find("- Triangulate:"), std::string::npos);
    EXPECT_NE(out.find("- BevelChamfer:"), std::string::npos);
    EXPECT_NE(out.find("- Remesh:"), std::string::npos);
    EXPECT_NE(out.find("cleanup_requested: true"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterModelCleanupShowsPreAndPostOverlaySnapshots)
{
    std::string out = runCommand(appPath + " --starter-model Sphere --cleanup 2>&1");
    EXPECT_NE(out.find("diagnostics overlay [pre-cleanup]"), std::string::npos);
    EXPECT_NE(out.find("diagnostics overlay [post-cleanup]"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterModelWithoutCleanupOmitsPostCleanupSnapshot)
{
    std::string out = runCommand(appPath + " --starter-model Sphere 2>&1");
    EXPECT_NE(out.find("cleanup_requested: false"), std::string::npos);
    EXPECT_NE(out.find("diagnostics overlay [pre-cleanup]"), std::string::npos);
    EXPECT_EQ(out.find("diagnostics overlay [post-cleanup]"), std::string::npos);
    EXPECT_EQ(out.find("- BevelChamfer:"), std::string::npos);
    EXPECT_EQ(out.find("- Remesh:"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, StarterModelCleanupExitCodeReflectsSessionOutcome)
{
    // Documented contract: app exits 0 on session.success=true, 1 otherwise.
    // CG-1 (kernel-contract-gaps.md) is closed in PR-22: quickCleanup now
    // succeeds for every starter primitive, so the app must exit 0.
    int exitCode = runCommandReturnCode(appPath + " --starter-model Sphere --cleanup > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, HelpListsCleanupFlag)
{
    std::string out = runCommand(appPath + " --help 2>&1");
    EXPECT_NE(out.find("--cleanup"), std::string::npos);
}

// --- Week 3 Day 4: narrow asset I/O for starter slice (PR-14) --------------

TEST_F(ToolingAppSmokeTests, StarterModelExportsObjMesh)
{
    auto tmp = makeTempDir("export_obj");
    const auto out_path = tmp / "box.obj";
    std::string out = runCommand(appPath + " --starter-model Box --export-mesh "
                                 + out_path.string() + " 2>&1");
    EXPECT_NE(out.find("mesh export:"), std::string::npos);
    EXPECT_NE(out.find("format: OBJ"), std::string::npos);
    EXPECT_NE(out.find("success: true"), std::string::npos);
    EXPECT_NE(out.find("vertices_written: 8"), std::string::npos);
    EXPECT_NE(out.find("faces_written: 6"), std::string::npos);
    EXPECT_TRUE(std::filesystem::exists(out_path));
    EXPECT_GT(std::filesystem::file_size(out_path), 0u);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, StarterModelExportsPlyMeshWhenExtensionMatches)
{
    auto tmp = makeTempDir("export_ply");
    const auto out_path = tmp / "box.ply";
    std::string out = runCommand(appPath + " --starter-model Box --export-mesh "
                                 + out_path.string() + " 2>&1");
    EXPECT_NE(out.find("format: PLY"), std::string::npos);
    EXPECT_NE(out.find("success: true"), std::string::npos);
    EXPECT_TRUE(std::filesystem::exists(out_path));
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ImportMeshReadsExportedObj)
{
    auto tmp = makeTempDir("roundtrip");
    const auto out_path = tmp / "box.obj";
    // Stage 1: export.
    int exportExit = runCommandReturnCode(appPath + " --starter-model Box --export-mesh "
                                          + out_path.string() + " > /dev/null 2>&1");
    ASSERT_EQ(exportExit, 0);
    ASSERT_TRUE(std::filesystem::exists(out_path));

    // Stage 2: import the same file standalone.
    std::string out = runCommand(appPath + " --import-mesh " + out_path.string() + " 2>&1");
    EXPECT_NE(out.find("Mesh import"), std::string::npos);
    EXPECT_NE(out.find("success: true"), std::string::npos);
    EXPECT_NE(out.find("vertices_read: 8"), std::string::npos);
    EXPECT_NE(out.find("faces_read: 6"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ImportMeshMissingFileFailsCleanly)
{
    auto tmp = makeTempDir("import_missing");
    int exitCode = runCommandReturnCode(appPath + " --import-mesh "
                                        + (tmp / "nope.obj").string() + " > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 1);
    std::string out = runCommand(appPath + " --import-mesh "
                                 + (tmp / "nope.obj").string() + " 2>&1");
    EXPECT_NE(out.find("success: false"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ImportMeshMissingArgumentReturnsUsageError)
{
    int exitCode = runCommandReturnCode(appPath + " --import-mesh > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 2);
}

TEST_F(ToolingAppSmokeTests, StarterModelExportsDiagnosticsSnapshot)
{
    auto tmp = makeTempDir("export_diag");
    const auto out_path = tmp / "diag.txt";
    std::string out = runCommand(appPath + " --starter-model Sphere --export-diagnostics "
                                 + out_path.string() + " 2>&1");
    EXPECT_NE(out.find("diagnostics export:"), std::string::npos);
    EXPECT_NE(out.find("success: true"), std::string::npos);
    ASSERT_TRUE(std::filesystem::exists(out_path));

    std::ifstream in(out_path);
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string contents = buf.str();
    EXPECT_NE(contents.find("primitive: Sphere"), std::string::npos);
    EXPECT_NE(contents.find("non_manifold_edges:"), std::string::npos);
    EXPECT_NE(contents.find("boundary_edges:"), std::string::npos);
    EXPECT_NE(contents.find("degenerate_faces:"), std::string::npos);
    EXPECT_NE(contents.find("isolated_vertices:"), std::string::npos);
    EXPECT_NE(contents.find("workflow_step_count:"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, LoadSceneMissingFileFailsCleanly)
{
    auto tmp = makeTempDir("scene_missing");
    int exitCode = runCommandReturnCode(appPath + " --load-scene "
                                        + (tmp / "nope.bin").string() + " > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 1);
    std::string out = runCommand(appPath + " --load-scene "
                                 + (tmp / "nope.bin").string() + " 2>&1");
    EXPECT_NE(out.find("Scene asset load"), std::string::npos);
    EXPECT_NE(out.find("success: false"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, LoadSceneRoundTripsSavedAsset)
{
    auto tmp = makeTempDir("scene_round");
    const auto scenePath = tmp / "scene.bin";

    // Save a tiny scene via the public API so we don't depend on bundled fixtures.
    {
        nexus::asset::SceneAsset scene;
        scene.setSceneName("starter_scene");
        const auto saveReport = scene.save(scenePath.string());
        ASSERT_TRUE(saveReport.isSuccess()) << "scene save failed";
        ASSERT_TRUE(std::filesystem::exists(scenePath));
    }

    std::string out = runCommand(appPath + " --load-scene " + scenePath.string() + " 2>&1");
    EXPECT_NE(out.find("success: true"), std::string::npos);
    EXPECT_NE(out.find("scene_name: starter_scene"), std::string::npos);
    EXPECT_NE(out.find("entry_count: 0"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, LoadSceneMissingArgumentReturnsUsageError)
{
    int exitCode = runCommandReturnCode(appPath + " --load-scene > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 2);
}

TEST_F(ToolingAppSmokeTests, HelpListsAssetIoFlags)
{
    std::string out = runCommand(appPath + " --help 2>&1");
    EXPECT_NE(out.find("--import-mesh"), std::string::npos);
    EXPECT_NE(out.find("--export-mesh"), std::string::npos);
    EXPECT_NE(out.find("--export-diagnostics"), std::string::npos);
    EXPECT_NE(out.find("--load-scene"), std::string::npos);
}

// --- Week 3 Day 5: integration + contract-gap log (PR-15) -----------------

TEST_F(ToolingAppSmokeTests, HelpListsSessionSummaryFlag)
{
    std::string out = runCommand(appPath + " --help 2>&1");
    EXPECT_NE(out.find("--session-summary"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, SessionSummaryOmittedByDefaultForStarterModel)
{
    std::string out = runCommand(appPath + " --starter-model Box 2>&1");
    EXPECT_EQ(out.find("session summary:"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, SessionSummaryEmittedForStarterModelWhenRequested)
{
    std::string out = runCommand(appPath + " --starter-model Box --session-summary 2>&1");
    EXPECT_NE(out.find("session summary:"), std::string::npos);
    EXPECT_NE(out.find("kind: starter-model"), std::string::npos);
    EXPECT_NE(out.find("label: Box"), std::string::npos);
    EXPECT_NE(out.find("ready: true"), std::string::npos);
    EXPECT_NE(out.find("session_success: true"), std::string::npos);
    EXPECT_NE(out.find("diagnostics_valid: true"), std::string::npos);
    EXPECT_NE(out.find("artifact_count: 0"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, SessionSummaryEmittedForImportMesh)
{
    auto tmp = makeTempDir("session_import");
    const auto out_path = tmp / "box.obj";
    ASSERT_EQ(0, runCommandReturnCode(appPath + " --starter-model Box --export-mesh "
                                      + out_path.string() + " > /dev/null 2>&1"));

    std::string out = runCommand(appPath + " --import-mesh " + out_path.string()
                                 + " --session-summary 2>&1");
    EXPECT_NE(out.find("session summary:"), std::string::npos);
    EXPECT_NE(out.find("kind: import-mesh"), std::string::npos);
    EXPECT_NE(out.find("artifact_count: 1"), std::string::npos);
    EXPECT_NE(out.find(out_path.string()), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, SessionSummaryEmittedForLoadScene)
{
    auto tmp = makeTempDir("session_scene");
    const auto scenePath = tmp / "scene.bin";
    {
        nexus::asset::SceneAsset scene;
        scene.setSceneName("integration_scene");
        ASSERT_TRUE(scene.save(scenePath.string()).isSuccess());
    }

    std::string out = runCommand(appPath + " --load-scene " + scenePath.string()
                                 + " --session-summary 2>&1");
    EXPECT_NE(out.find("kind: load-scene"), std::string::npos);
    EXPECT_NE(out.find("session_success: true"), std::string::npos);
    EXPECT_NE(out.find("artifact_count: 1"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

// End-to-end starter-model integration smoke: create -> quick cleanup ->
// diagnostics inspection -> mesh export -> diagnostics export -> session
// summary. This is the Week-3-acceptance-check fixture and intentionally
// exercises every public surface the starter slice exposes.
TEST_F(ToolingAppSmokeTests, StarterModelEndToEndIntegrationSmoke)
{
    auto tmp = makeTempDir("starter_e2e");
    const auto meshPath = tmp / "starter.obj";
    const auto diagPath = tmp / "starter_diag.txt";

    std::string cmd = appPath
                      + " --starter-model Box"
                      + " --cleanup"
                      + " --export-mesh " + meshPath.string()
                      + " --export-diagnostics " + diagPath.string()
                      + " --session-summary 2>&1";
    std::string out = runCommand(cmd);

    // 1. starter session header + workflow step kinds were exercised.
    EXPECT_NE(out.find("Starter model session"), std::string::npos);
    EXPECT_NE(out.find("- Init: ok"), std::string::npos);
    EXPECT_NE(out.find("- BevelChamfer:"), std::string::npos);
    EXPECT_NE(out.find("- Remesh:"), std::string::npos);
    EXPECT_NE(out.find("- RebuildNormals: ok"), std::string::npos);

    // 2. both pre- and post-cleanup diagnostics overlays were printed.
    EXPECT_NE(out.find("[pre-cleanup]"), std::string::npos);
    EXPECT_NE(out.find("[post-cleanup]"), std::string::npos);

    // 3. mesh + diagnostics artifacts were written.
    EXPECT_NE(out.find("mesh export:"), std::string::npos);
    EXPECT_NE(out.find("diagnostics export:"), std::string::npos);
    ASSERT_TRUE(std::filesystem::exists(meshPath));
    EXPECT_GT(std::filesystem::file_size(meshPath), 0u);
    ASSERT_TRUE(std::filesystem::exists(diagPath));
    EXPECT_GT(std::filesystem::file_size(diagPath), 0u);

    // 4. consolidated session summary lists both artifact paths.
    EXPECT_NE(out.find("session summary:"), std::string::npos);
    EXPECT_NE(out.find("kind: starter-model"), std::string::npos);
    EXPECT_NE(out.find("artifact_count: 2"), std::string::npos);
    EXPECT_NE(out.find(meshPath.string()), std::string::npos);
    EXPECT_NE(out.find(diagPath.string()), std::string::npos);

    // 5. CG-1 (kernel-contract-gaps.md) is closed in PR-22: the starter
    //    cleanup path must now exit 0 for every starter primitive.
    int exitCode = runCommandReturnCode(cmd + " > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);

    std::filesystem::remove_all(tmp);
}

// --- Week 4 Day 1: editor-state seams scaffold (PR-16) ---------------------

TEST_F(ToolingAppSmokeTests, HelpListsEditorSeamsDemoFlag)
{
    std::string out = runCommand(appPath + " --help 2>&1");
    EXPECT_NE(out.find("--editor-seams-demo"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, EditorSeamsDemoExitsZero)
{
    int exitCode = runCommandReturnCode(appPath + " --editor-seams-demo > /dev/null 2>&1");
    EXPECT_EQ(exitCode, 0);
}

TEST_F(ToolingAppSmokeTests, EditorSeamsDemoEmitsAllSeamLabels)
{
    std::string out = runCommand(appPath + " --editor-seams-demo 2>&1");
    EXPECT_NE(out.find("Editor seams demo"), std::string::npos);
    EXPECT_NE(out.find("document.label:"), std::string::npos);
    EXPECT_NE(out.find("document.session_kind:"), std::string::npos);
    EXPECT_NE(out.find("document.dirty:"), std::string::npos);
    EXPECT_NE(out.find("selection.size:"), std::string::npos);
    EXPECT_NE(out.find("undo_depth:"), std::string::npos);
    EXPECT_NE(out.find("redo_depth:"), std::string::npos);
    EXPECT_NE(out.find("can_undo:"), std::string::npos);
    EXPECT_NE(out.find("can_redo:"), std::string::npos);
    EXPECT_NE(out.find("max_undo_depth:"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, EditorSeamsDemoExercisesEachStep)
{
    std::string out = runCommand(appPath + " --editor-seams-demo 2>&1");
    EXPECT_NE(out.find("[initial]"), std::string::npos);
    EXPECT_NE(out.find("[after_set_label]"), std::string::npos);
    EXPECT_NE(out.find("[after_add_selection]"), std::string::npos);
    EXPECT_NE(out.find("[after_undo_selection]"), std::string::npos);
    EXPECT_NE(out.find("[after_undo_label]"), std::string::npos);
    EXPECT_NE(out.find("[after_redo_label]"), std::string::npos);
    EXPECT_NE(out.find("[after_branch_new_command]"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, EditorSeamsDemoSelectionIdsAreDeterministic)
{
    std::string out = runCommand(appPath + " --editor-seams-demo 2>&1");
    const auto addPos = out.find("[after_add_selection]");
    ASSERT_NE(addPos, std::string::npos);
    const auto nextPos = out.find("[after_undo_selection]", addPos);
    ASSERT_NE(nextPos, std::string::npos);
    const std::string block = out.substr(addPos, nextPos - addPos);

    const auto p0 = block.find("- v0");
    const auto p1 = block.find("- v1");
    const auto p2 = block.find("- v2");
    ASSERT_NE(p0, std::string::npos);
    ASSERT_NE(p1, std::string::npos);
    ASSERT_NE(p2, std::string::npos);
    EXPECT_LT(p0, p1);
    EXPECT_LT(p1, p2);
    EXPECT_NE(block.find("selection.size: 3"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, EditorSeamsDemoRestoresDocumentLabelOnUndo)
{
    std::string out = runCommand(appPath + " --editor-seams-demo 2>&1");
    const auto undoLabelPos = out.find("[after_undo_label]");
    ASSERT_NE(undoLabelPos, std::string::npos);
    const auto endPos = out.find("[after_redo_label]", undoLabelPos);
    ASSERT_NE(endPos, std::string::npos);
    const std::string block = out.substr(undoLabelPos, endPos - undoLabelPos);

    // Undoing past the label command restores the initial "untitled" label
    // and drops dirty back to false (because the label command captured the
    // previous dirty=false state at apply time).
    EXPECT_NE(block.find("document.label: untitled"), std::string::npos);
    EXPECT_NE(block.find("document.dirty: false"), std::string::npos);
    EXPECT_NE(block.find("undo_depth: 0"), std::string::npos);
    EXPECT_NE(block.find("redo_depth: 2"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, EditorSeamsDemoBranchClearsRedoStack)
{
    std::string out = runCommand(appPath + " --editor-seams-demo 2>&1");
    const auto branchPos = out.find("[after_branch_new_command]");
    ASSERT_NE(branchPos, std::string::npos);
    const std::string block = out.substr(branchPos);
    EXPECT_NE(block.find("redo_depth: 0"), std::string::npos);
    EXPECT_NE(block.find("can_redo: false"), std::string::npos);
    EXPECT_NE(block.find("- e0"), std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
//  PR-17 — Week 4 Day 2: read-only session/scene panel
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(ToolingAppSmokeTests, HelpListsSessionPanelFlag)
{
    std::string out = runCommand(appPath + " --help 2>&1");
    EXPECT_NE(out.find("--session-panel"), std::string::npos);
    EXPECT_NE(out.find("--panel-mesh"), std::string::npos);
    EXPECT_NE(out.find("--panel-scene"), std::string::npos);
    EXPECT_NE(out.find("--panel-diagnostics"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, SessionPanelEmptyStateExitsZero)
{
    int rc = runCommandReturnCode(appPath + " --session-panel > /dev/null 2>&1");
    EXPECT_EQ(rc, 0);
}

TEST_F(ToolingAppSmokeTests, SessionPanelEmptyStateIncludesAllPaneLabels)
{
    std::string out = runCommand(appPath + " --session-panel 2>&1");
    // All four panes must always render so the user gets a consistent layout
    // even when no on-disk artifacts are supplied.
    EXPECT_NE(out.find("[document]"), std::string::npos);
    EXPECT_NE(out.find("[scene outline]"), std::string::npos);
    EXPECT_NE(out.find("[workflow status]"), std::string::npos);
    EXPECT_NE(out.find("[diagnostics summary]"), std::string::npos);
    // Empty-state placeholders for the inputs.
    EXPECT_NE(out.find("label: untitled"), std::string::npos);
    EXPECT_NE(out.find("session_kind: none"), std::string::npos);
    EXPECT_NE(out.find("scene: (none)"), std::string::npos);
    EXPECT_NE(out.find("mesh: (none)"), std::string::npos);
    EXPECT_NE(out.find("source: (none)"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, SessionPanelRendersFromMeshAndDiagnostics)
{
    auto tmp = makeTempDir("panel_mesh_diag");
    const auto meshPath = tmp / "box.obj";
    const auto diagPath = tmp / "diag.txt";

    // Produce both artifacts via the starter-model subcommand so the panel
    // reads the same on-disk format the app would produce in practice.
    int genRc = runCommandReturnCode(
        appPath + " --starter-model Box --export-mesh " + meshPath.string()
        + " --export-diagnostics " + diagPath.string() + " > /dev/null 2>&1");
    ASSERT_EQ(genRc, 0);
    ASSERT_TRUE(std::filesystem::exists(meshPath));
    ASSERT_TRUE(std::filesystem::exists(diagPath));

    std::string out = runCommand(
        appPath + " --session-panel --panel-mesh " + meshPath.string()
        + " --panel-diagnostics " + diagPath.string() + " 2>&1");

    // Workflow pane reflects the imported mesh (Box exports 8 verts / 6 quads).
    EXPECT_NE(out.find("import_success: true"), std::string::npos);
    EXPECT_NE(out.find("import_valid: true"), std::string::npos);
    EXPECT_NE(out.find("vertices: 8"), std::string::npos);
    EXPECT_NE(out.find("faces: 6"), std::string::npos);
    // Diagnostics pane reflects parsed key:value snapshot.
    EXPECT_NE(out.find("read_success: true"), std::string::npos);
    EXPECT_NE(out.find("non_manifold_edges: 0"), std::string::npos);
    EXPECT_NE(out.find("boundary_edges: 0"), std::string::npos);
    EXPECT_NE(out.find("degenerate_faces: 0"), std::string::npos);
    EXPECT_NE(out.find("isolated_vertices: 0"), std::string::npos);
    // Document label derives from mesh filename stem when only mesh is provided.
    EXPECT_NE(out.find("label: box"), std::string::npos);
    EXPECT_NE(out.find("session_kind: mesh"), std::string::npos);

    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, SessionPanelRendersFromSavedScene)
{
    auto tmp = makeTempDir("panel_scene");
    const auto scenePath = tmp / "scene.bin";

    {
        nexus::asset::SceneAsset scene;
        scene.setSceneName("panel_scene_fixture");
        const auto saveReport = scene.save(scenePath.string());
        ASSERT_TRUE(saveReport.isSuccess()) << "scene save failed";
    }

    std::string out = runCommand(
        appPath + " --session-panel --panel-scene " + scenePath.string() + " 2>&1");

    EXPECT_NE(out.find("session_kind: scene"), std::string::npos);
    EXPECT_NE(out.find("label: panel_scene_fixture"), std::string::npos);
    EXPECT_NE(out.find("scene: panel_scene_fixture"), std::string::npos);
    EXPECT_NE(out.find("load_success: true"), std::string::npos);
    EXPECT_NE(out.find("load_valid: true"), std::string::npos);
    EXPECT_NE(out.find("entry_count: 0"), std::string::npos);
    // Empty entries case must surface an explicit placeholder, not silence.
    EXPECT_NE(out.find("(no entries)"), std::string::npos);

    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, SessionPanelMissingPanelMeshArgumentReturnsUsageError)
{
    int rc = runCommandReturnCode(appPath + " --session-panel --panel-mesh > /dev/null 2>&1");
    EXPECT_EQ(rc, 2);
}

TEST_F(ToolingAppSmokeTests, SessionPanelMissingDiagnosticsFileMarksReadFailure)
{
    auto tmp = makeTempDir("panel_missing_diag");
    const auto missing = tmp / "does_not_exist.txt";
    ASSERT_FALSE(std::filesystem::exists(missing));

    std::string out = runCommand(
        appPath + " --session-panel --panel-diagnostics " + missing.string() + " 2>&1");

    // Panel should still render (exit 0) and explicitly mark the read as failed
    // rather than crashing or omitting the pane.
    EXPECT_NE(out.find("[diagnostics summary]"), std::string::npos);
    EXPECT_NE(out.find("read_success: false"), std::string::npos);

    std::filesystem::remove_all(tmp);
}

// ─────────────────────────────────────────────────────────────────────────────
//  PR-18 — Week 4 Day 3: persistence and recovery basics
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(ToolingAppSmokeTests, HelpListsPersistenceFlags)
{
    std::string out = runCommand(appPath + " --help 2>&1");
    EXPECT_NE(out.find("--save-session-state"), std::string::npos);
    EXPECT_NE(out.find("--restore-session-state"), std::string::npos);
    EXPECT_NE(out.find("--append-state-log"), std::string::npos);
    EXPECT_NE(out.find("--show-state-log"), std::string::npos);
    EXPECT_NE(out.find("--state-dir"), std::string::npos);
    EXPECT_NE(out.find("--session-kind"), std::string::npos);
}

TEST_F(ToolingAppSmokeTests, SaveSessionStateRequiresSessionKind)
{
    auto tmp = makeTempDir("persist_missing_kind");
    int rc = runCommandReturnCode(
        appPath + " --save-session-state --state-dir " + tmp.string()
        + " > /dev/null 2>&1");
    EXPECT_EQ(rc, 2);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, SaveSessionStateWritesDeterministicFile)
{
    auto tmp = makeTempDir("persist_save");
    int rc = runCommandReturnCode(
        appPath + " --save-session-state --session-kind starter-model"
        + " --session-label cube-demo --panel-mesh /tmp/box.obj"
        + " --state-dir " + tmp.string() + " > /dev/null 2>&1");
    EXPECT_EQ(rc, 0);
    const auto stateFile = tmp / "last_session.txt";
    ASSERT_TRUE(std::filesystem::exists(stateFile));

    // File contents should be the documented key:value layout.
    std::ifstream in(stateFile);
    std::ostringstream buf;
    buf << in.rdbuf();
    const std::string contents = buf.str();
    EXPECT_NE(contents.find("session_kind: starter-model"), std::string::npos);
    EXPECT_NE(contents.find("session_label: cube-demo"), std::string::npos);
    EXPECT_NE(contents.find("last_mesh: /tmp/box.obj"), std::string::npos);
    EXPECT_NE(contents.find("saved_at_unix:"), std::string::npos);

    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, SaveAndRestoreSessionStateRoundTrip)
{
    auto tmp = makeTempDir("persist_round");
    int saveRc = runCommandReturnCode(
        appPath + " --save-session-state --session-kind load-scene"
        + " --session-label fixture --panel-scene /tmp/x.scene"
        + " --panel-diagnostics /tmp/x.diag --state-dir " + tmp.string()
        + " > /dev/null 2>&1");
    ASSERT_EQ(saveRc, 0);

    std::string out = runCommand(
        appPath + " --restore-session-state --state-dir " + tmp.string() + " 2>&1");
    EXPECT_NE(out.find("state_exists: true"), std::string::npos);
    EXPECT_NE(out.find("read_success: true"), std::string::npos);
    EXPECT_NE(out.find("session_kind: load-scene"), std::string::npos);
    EXPECT_NE(out.find("session_label: fixture"), std::string::npos);
    EXPECT_NE(out.find("last_scene: /tmp/x.scene"), std::string::npos);
    EXPECT_NE(out.find("last_diagnostics: /tmp/x.diag"), std::string::npos);
    EXPECT_NE(out.find("unknown_key_count: 0"), std::string::npos);
    EXPECT_NE(out.find("malformed_line_count: 0"), std::string::npos);

    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, RestoreSessionStateAbsentDirRendersEmptyState)
{
    auto tmp = makeTempDir("persist_absent");
    const auto missingDir = tmp / "never_created";
    // Intentionally do not create the directory.
    std::string out = runCommand(
        appPath + " --restore-session-state --state-dir " + missingDir.string()
        + " 2>&1");
    int rc = runCommandReturnCode(
        appPath + " --restore-session-state --state-dir " + missingDir.string()
        + " > /dev/null 2>&1");
    EXPECT_EQ(rc, 0) << "missing state dir must not crash the app";
    EXPECT_NE(out.find("state_exists: false"), std::string::npos);
    EXPECT_NE(out.find("session_kind: (missing)"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, RestoreSessionStateToleratesTruncatedAndUnknownKeys)
{
    auto tmp = makeTempDir("persist_stale");
    const auto stateFile = tmp / "last_session.txt";
    // Hand-written stale file: truncated payload + malformed line + unknown key.
    {
        std::ofstream out(stateFile);
        out << "session_kind: stale-kind\n";
        out << "MALFORMED_NO_COLON\n";
        out << "bogus_field: ignore-me\n";
        out << "session_label: partial"; // no trailing newline → truncation guard
    }
    std::string output = runCommand(
        appPath + " --restore-session-state --state-dir " + tmp.string() + " 2>&1");
    int rc = runCommandReturnCode(
        appPath + " --restore-session-state --state-dir " + tmp.string()
        + " > /dev/null 2>&1");
    EXPECT_EQ(rc, 0);
    EXPECT_NE(output.find("session_kind: stale-kind"), std::string::npos);
    EXPECT_NE(output.find("session_label: partial"), std::string::npos);
    EXPECT_NE(output.find("last_mesh: (missing)"), std::string::npos);
    EXPECT_NE(output.find("unknown_key_count: 1"), std::string::npos);
    EXPECT_NE(output.find("unknown_key: bogus_field"), std::string::npos);
    EXPECT_NE(output.find("malformed_line_count: 1"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, AppendStateLogPersistsAcrossInvocations)
{
    auto tmp = makeTempDir("persist_log");
    int rc1 = runCommandReturnCode(
        appPath + " --append-state-log first-event --state-dir " + tmp.string()
        + " > /dev/null 2>&1");
    int rc2 = runCommandReturnCode(
        appPath + " --append-state-log second-event --state-dir " + tmp.string()
        + " > /dev/null 2>&1");
    ASSERT_EQ(rc1, 0);
    ASSERT_EQ(rc2, 0);

    std::string out = runCommand(
        appPath + " --show-state-log --state-dir " + tmp.string() + " 2>&1");
    EXPECT_NE(out.find("log_exists: true"), std::string::npos);
    EXPECT_NE(out.find("line_count: 2"), std::string::npos);
    EXPECT_NE(out.find("first-event"), std::string::npos);
    EXPECT_NE(out.find("second-event"), std::string::npos);

    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, ShowStateLogAbsentFileReportsEmpty)
{
    auto tmp = makeTempDir("persist_log_absent");
    std::string out = runCommand(
        appPath + " --show-state-log --state-dir " + tmp.string() + " 2>&1");
    EXPECT_NE(out.find("log_exists: false"), std::string::npos);
    EXPECT_NE(out.find("line_count: 0"), std::string::npos);
    std::filesystem::remove_all(tmp);
}

TEST_F(ToolingAppSmokeTests, AppendStateLogMissingMessageReturnsUsageError)
{
    auto tmp = makeTempDir("persist_log_no_msg");
    int rc = runCommandReturnCode(
        appPath + " --append-state-log --state-dir " + tmp.string()
        + " > /dev/null 2>&1");
    // --append-state-log consumes "--state-dir" as the message, so the next arg
    // (--state-dir's value) becomes orphaned. Either result (2 or non-zero) is
    // acceptable as a usage failure; the contract is "does not silently succeed".
    EXPECT_NE(rc, 0);
    std::filesystem::remove_all(tmp);
}
