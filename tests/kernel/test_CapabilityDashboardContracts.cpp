#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace nexus::testing {

namespace fs = std::filesystem;

namespace {

std::string shellQuote(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('\'');
    for (char c : value) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

std::string readAll(const fs::path& path)
{
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

int runDashboard(const fs::path& scriptPath,
                 const fs::path& dashboardPath,
                 const fs::path& knownGapsPath,
                 const fs::path& manifestOverride = {},
                 const fs::path& artifactRootOverride = {})
{
    std::string cmd;
    if (!manifestOverride.empty()) {
        cmd += "NEXUS_SCENARIO_MANIFEST=" + shellQuote(manifestOverride.string()) + " ";
    }
    if (!artifactRootOverride.empty()) {
        cmd += "NEXUS_SCENARIO_ARTIFACT_ROOT=" + shellQuote(artifactRootOverride.string()) + " ";
    }
    cmd += "bash " + shellQuote(scriptPath.string()) +
           " --skip-gates --output " + shellQuote(dashboardPath.string()) +
           " --known-gaps-output " + shellQuote(knownGapsPath.string());
    return std::system(cmd.c_str());
}

} // namespace

TEST(CapabilityDashboardContracts, IncludesAllPhase0Subsystems)
{
    const fs::path testsRoot = fs::path(NEXUS_TESTS_ROOT);
    const fs::path repoRoot = testsRoot.parent_path();
    const fs::path dashboardScript = repoRoot / "tools" / "capability_dashboard.sh";
    const fs::path dashboardPath = repoRoot / "build" / "capability_dashboard.json";
    const fs::path knownGapsPath = repoRoot / "build" / "known_gaps.json";

    ASSERT_TRUE(fs::exists(dashboardScript)) << "missing script: " << dashboardScript;

    const int rc = runDashboard(dashboardScript, dashboardPath, knownGapsPath);
    ASSERT_EQ(rc, 0) << "capability dashboard generation failed";

    ASSERT_TRUE(fs::exists(dashboardPath)) << "missing dashboard: " << dashboardPath;
    const std::string json = readAll(dashboardPath);

    const std::array<const char*, 8> subsystems = {
        "Graphics Backend",
        "Render Pipeline",
        "Scene and Eval Graph",
        "Geometry Core",
        "Parametric Foundation",
        "Simulation Core",
        "Gaussian and Neural Integration",
        "Asset and Automation",
    };

    for (const char* subsystem : subsystems) {
        EXPECT_NE(json.find(std::string("\"name\": \"") + subsystem + "\""), std::string::npos)
            << "missing subsystem in dashboard: " << subsystem;
    }

    EXPECT_NE(json.find("\"scenario_checks\":"), std::string::npos)
        << "subsystem-level scenario attribution block missing";
    EXPECT_NE(json.find("\"asset_graph\":"), std::string::npos)
        << "missing asset_graph scenario check group for Asset and Automation subsystem";
    EXPECT_NE(json.find("\"automation_pipeline\":"), std::string::npos)
        << "missing automation_pipeline scenario check group for Asset and Automation subsystem";
    EXPECT_NE(json.find("asset_importer_dependency_mode_scene"), std::string::npos)
        << "missing SceneAssetImporter dependency-mode scenario id in split scenario checks";
    EXPECT_NE(json.find("asset_automation_pipeline_scene"), std::string::npos)
        << "missing automation pipeline scenario id in split scenario checks";
    EXPECT_NE(json.find("\"asset_graph\": {\n            \"required\":"), std::string::npos)
        << "asset_graph block is missing expected shape";
    EXPECT_NE(json.find("\"automation_pipeline\": {\n            \"required\":"), std::string::npos)
        << "automation_pipeline block is missing expected shape";
    EXPECT_NE(json.find("\"deterministic_signature\": \""), std::string::npos)
        << "deterministic signatures should be emitted for subsystem and split groups";
}

TEST(CapabilityDashboardContracts, ScenarioCanonicalFieldsAndCoverageGapContract)
{
    const fs::path testsRoot = fs::path(NEXUS_TESTS_ROOT);
    const fs::path repoRoot = testsRoot.parent_path();
    const fs::path dashboardPath = repoRoot / "build" / "capability_dashboard.json";
    const fs::path knownGapsPath = repoRoot / "build" / "known_gaps.json";

    ASSERT_TRUE(fs::exists(dashboardPath)) << "missing dashboard: " << dashboardPath;
    ASSERT_TRUE(fs::exists(knownGapsPath)) << "missing known gaps: " << knownGapsPath;

    const std::string dashboardJson = readAll(dashboardPath);
    const std::string knownGapsJson = readAll(knownGapsPath);

    EXPECT_NE(dashboardJson.find("\"canonical_target\": 8"), std::string::npos);
    EXPECT_NE(dashboardJson.find("\"canonical_total\":"), std::string::npos);

    EXPECT_EQ(knownGapsJson.find("P0-SCENARIO-COVERAGE"), std::string::npos)
        << "coverage gap should not be present when canonical scenario count is at least 8";
}

TEST(CapabilityDashboardContracts, TemporaryManifestBelowTargetEmitsCoverageGap)
{
    const fs::path testsRoot = fs::path(NEXUS_TESTS_ROOT);
    const fs::path repoRoot = testsRoot.parent_path();
    const fs::path dashboardScript = repoRoot / "tools" / "capability_dashboard.sh";

    const fs::path tempRoot = repoRoot / "build" / "dashboard_contract_tmp";
    const fs::path tempManifest = tempRoot / "manifest_under_target.tsv";
    const fs::path tempDashboard = tempRoot / "capability_dashboard_under_target.json";
    const fs::path tempKnownGaps = tempRoot / "known_gaps_under_target.json";

    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot);

    std::ofstream out(tempManifest, std::ios::trunc);
    ASSERT_TRUE(out.good());
    out << "# scenario_id\ttitle\tbackend\tscript\tdescription\n";
    out << "render_scheduler_baseline\tRender Scheduler Baseline\tnull\tscripts/scenarios/render_scheduler_baseline.sh\tMinimal entry\n";
    out << "nodescene_reconstruction_diagnostics\tNodeScene Reconstruction Diagnostics\tnull\tscripts/scenarios/nodescene_reconstruction_diagnostics.sh\tMinimal entry\n";
    out.close();

    const int rc = runDashboard(dashboardScript, tempDashboard, tempKnownGaps, tempManifest);
    ASSERT_EQ(rc, 0) << "dashboard generation failed for temporary manifest override";

    const std::string knownGapsJson = readAll(tempKnownGaps);
    EXPECT_NE(knownGapsJson.find("P0-SCENARIO-COVERAGE"), std::string::npos)
        << "expected P0-SCENARIO-COVERAGE when manifest has fewer than 8 entries";
}

TEST(CapabilityDashboardContracts, ValidatorFailsWhenDashboardKnownGapsDiverge)
{
    const fs::path testsRoot = fs::path(NEXUS_TESTS_ROOT);
    const fs::path repoRoot = testsRoot.parent_path();
    const fs::path dashboardScript = repoRoot / "tools" / "capability_dashboard.sh";
    const fs::path validatorScript = repoRoot / "tools" / "validate_dashboard_artifacts.sh";

    const fs::path tempRoot = repoRoot / "build" / "dashboard_validation_tmp";
    const fs::path tempDashboard = tempRoot / "capability_dashboard.json";
    const fs::path tempKnownGaps = tempRoot / "known_gaps.json";

    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot);

    const int dashboardRc = runDashboard(dashboardScript, tempDashboard, tempKnownGaps);
    ASSERT_EQ(dashboardRc, 0) << "dashboard generation failed";

    const std::string validateOkCmd =
        "bash " + shellQuote(validatorScript.string()) +
        " --dashboard " + shellQuote(tempDashboard.string()) +
        " --known-gaps " + shellQuote(tempKnownGaps.string());
    const int validateOkRc = std::system(validateOkCmd.c_str());
    ASSERT_EQ(validateOkRc, 0) << "baseline validator run should pass";

    const std::string mutateCmd =
        "python3 -c \"import json,pathlib; p=pathlib.Path(r'" + tempDashboard.string() +
        "'); d=json.loads(p.read_text(encoding='utf-8')); d['known_gaps']=[]; p.write_text(json.dumps(d, indent=2)+\\\"\\n\\\", encoding='utf-8')\"";
    const int mutateRc = std::system(mutateCmd.c_str());
    ASSERT_EQ(mutateRc, 0) << "failed to mutate dashboard known_gaps";

    const int validateFailRc = std::system(validateOkCmd.c_str());
    EXPECT_NE(validateFailRc, 0)
        << "validator should fail when dashboard.known_gaps diverges from known_gaps.json";
}

TEST(CapabilityDashboardContracts, ValidatorFailsWhenRequiredScenarioFieldMissing)
{
    const fs::path testsRoot = fs::path(NEXUS_TESTS_ROOT);
    const fs::path repoRoot = testsRoot.parent_path();
    const fs::path dashboardScript = repoRoot / "tools" / "capability_dashboard.sh";
    const fs::path validatorScript = repoRoot / "tools" / "validate_dashboard_artifacts.sh";

    const fs::path tempRoot = repoRoot / "build" / "dashboard_required_field_tmp";
    const fs::path tempDashboard = tempRoot / "capability_dashboard.json";
    const fs::path tempKnownGaps = tempRoot / "known_gaps.json";

    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot);

    const int dashboardRc = runDashboard(dashboardScript, tempDashboard, tempKnownGaps);
    ASSERT_EQ(dashboardRc, 0) << "dashboard generation failed";

    const std::string mutateCmd =
        "python3 -c \"import json,pathlib; p=pathlib.Path(r'" + tempDashboard.string() +
        "'); d=json.loads(p.read_text(encoding='utf-8')); del d['scenarios']['missing']; p.write_text(json.dumps(d, indent=2)+\\\"\\n\\\", encoding='utf-8')\"";
    const int mutateRc = std::system(mutateCmd.c_str());
    ASSERT_EQ(mutateRc, 0) << "failed to mutate dashboard scenarios block";

    const std::string validateCmd =
        "bash " + shellQuote(validatorScript.string()) +
        " --dashboard " + shellQuote(tempDashboard.string()) +
        " --known-gaps " + shellQuote(tempKnownGaps.string());

    const int validateRc = std::system(validateCmd.c_str());
    EXPECT_NE(validateRc, 0)
        << "validator should fail when required scenarios.missing field is absent";
}

TEST(CapabilityDashboardContracts, UnexpectedArtifactsAreCountedButDoNotTriggerCanonicalScenarioFail)
{
    const fs::path testsRoot = fs::path(NEXUS_TESTS_ROOT);
    const fs::path repoRoot = testsRoot.parent_path();
    const fs::path dashboardScript = repoRoot / "tools" / "capability_dashboard.sh";

    const fs::path tempRoot = repoRoot / "build" / "dashboard_unexpected_artifacts_tmp";
    const fs::path tempManifest = tempRoot / "manifest.tsv";
    const fs::path tempArtifacts = tempRoot / "artifacts";
    const fs::path tempDashboard = tempRoot / "capability_dashboard.json";
    const fs::path tempKnownGaps = tempRoot / "known_gaps.json";

    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempArtifacts / "render_scheduler_baseline");
    fs::create_directories(tempArtifacts / "rogue_noncanonical");

    {
        std::ofstream manifestOut(tempManifest, std::ios::trunc);
        ASSERT_TRUE(manifestOut.good());
        manifestOut << "# scenario_id\ttitle\tbackend\tscript\tdescription\n";
        manifestOut << "render_scheduler_baseline\tRender Scheduler Baseline\tnull\tscripts/scenarios/render_scheduler_baseline.sh\tMinimal entry\n";
    }

    {
        std::ofstream summaryOut(tempArtifacts / "render_scheduler_baseline" / "summary.json", std::ios::trunc);
        ASSERT_TRUE(summaryOut.good());
        summaryOut << "{\n  \"scenario_id\": \"render_scheduler_baseline\",\n  \"status\": \"pass\"\n}\n";
    }
    {
        std::ofstream sigOut(tempArtifacts / "render_scheduler_baseline" / "deterministic_signature.txt", std::ios::trunc);
        ASSERT_TRUE(sigOut.good());
        sigOut << "canonical-pass-signature\n";
    }

    {
        std::ofstream summaryOut(tempArtifacts / "rogue_noncanonical" / "summary.json", std::ios::trunc);
        ASSERT_TRUE(summaryOut.good());
        summaryOut << "{\n  \"scenario_id\": \"rogue_noncanonical\",\n  \"status\": \"fail\"\n}\n";
    }
    {
        std::ofstream sigOut(tempArtifacts / "rogue_noncanonical" / "deterministic_signature.txt", std::ios::trunc);
        ASSERT_TRUE(sigOut.good());
        sigOut << "rogue-fail-signature\n";
    }

    const int rc = runDashboard(dashboardScript, tempDashboard, tempKnownGaps, tempManifest, tempArtifacts);
    ASSERT_EQ(rc, 0) << "dashboard generation failed for temporary artifact-root override";

    const std::string dashboardJson = readAll(tempDashboard);
    const std::string knownGapsJson = readAll(tempKnownGaps);

    EXPECT_NE(dashboardJson.find("\"total\": 1"), std::string::npos)
        << "canonical total should drive scenario total";
    EXPECT_NE(dashboardJson.find("\"passed\": 1"), std::string::npos)
        << "canonical passed count should ignore rogue artifact";
    EXPECT_NE(dashboardJson.find("\"failed\": 0"), std::string::npos)
        << "canonical failed count should ignore rogue artifact";
    EXPECT_NE(dashboardJson.find("\"unexpected\": 1"), std::string::npos)
        << "unexpected artifact count should include rogue artifact";

    EXPECT_EQ(knownGapsJson.find("P0-SCENARIO-FAIL"), std::string::npos)
        << "non-canonical failing artifacts should not trigger P0-SCENARIO-FAIL";
    EXPECT_NE(knownGapsJson.find("P0-SCENARIO-UNEXPECTED"), std::string::npos)
        << "unexpected artifacts should trigger P0-SCENARIO-UNEXPECTED";
}

} // namespace nexus::testing