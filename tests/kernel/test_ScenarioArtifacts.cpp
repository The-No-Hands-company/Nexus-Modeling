#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace nexus::testing {

namespace fs = std::filesystem;

namespace {

struct ScenarioRow {
    std::string id;
    std::string title;
    std::string backend;
    std::string script;
    std::string description;
};

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

std::vector<ScenarioRow> readScenarioManifest(const fs::path& manifestPath)
{
    std::ifstream in(manifestPath);
    std::vector<ScenarioRow> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::array<std::string, 5> fields;
        std::size_t start = 0;
        int fieldIndex = 0;
        while (fieldIndex < 4) {
            const std::size_t tab = line.find('\t', start);
            if (tab == std::string::npos) {
                break;
            }
            fields[static_cast<std::size_t>(fieldIndex)] = line.substr(start, tab - start);
            start = tab + 1;
            ++fieldIndex;
        }
        if (fieldIndex != 4) {
            continue;
        }
        fields[4] = line.substr(start);

        if (fields[0].empty() || fields[3].empty()) {
            continue;
        }

        rows.push_back(ScenarioRow{
            fields[0],
            fields[1],
            fields[2],
            fields[3],
            fields[4],
        });
    }
    return rows;
}

} // namespace

TEST(ScenarioArtifacts, ManifestParsesAndMeetsCanonicalTarget)
{
    const fs::path testsRoot = fs::path(NEXUS_TESTS_ROOT);
    const fs::path repoRoot = testsRoot.parent_path();
    const fs::path manifestPath = repoRoot / "scripts" / "scenarios" / "manifest.tsv";

    ASSERT_TRUE(fs::exists(manifestPath)) << "missing manifest: " << manifestPath;

    const std::vector<ScenarioRow> rows = readScenarioManifest(manifestPath);
    EXPECT_GE(rows.size(), 8u) << "canonical scenario target is 8";

    for (const ScenarioRow& row : rows) {
        EXPECT_FALSE(row.id.empty()) << "scenario id must not be empty";
        EXPECT_FALSE(row.script.empty()) << "scenario script path must not be empty";
        const fs::path scriptPath = repoRoot / row.script;
        EXPECT_TRUE(fs::exists(scriptPath)) << "missing scenario script: " << scriptPath;
    }
}

TEST(ScenarioArtifacts, RunnerProducesRequiredArtifactsForManifestEntries)
{
    const fs::path testsRoot = fs::path(NEXUS_TESTS_ROOT);
    const fs::path repoRoot = testsRoot.parent_path();
    const fs::path manifestPath = repoRoot / "scripts" / "scenarios" / "manifest.tsv";
    const fs::path scenarioRunner = repoRoot / "tools" / "run_scenario.sh";

    ASSERT_TRUE(fs::exists(manifestPath)) << "missing manifest: " << manifestPath;
    ASSERT_TRUE(fs::exists(scenarioRunner)) << "missing scenario runner: " << scenarioRunner;

    const std::vector<ScenarioRow> rows = readScenarioManifest(manifestPath);
    ASSERT_FALSE(rows.empty()) << "scenario manifest has no entries";

    for (const ScenarioRow& row : rows) {
        const fs::path artifactDir = repoRoot / "build" / "scenario_artifacts" / row.id;
        std::error_code ec;
        fs::remove_all(artifactDir, ec);

        const std::string cmd =
            "bash " + shellQuote(scenarioRunner.string()) + " " + shellQuote(row.id);
        const int rc = std::system(cmd.c_str());
        ASSERT_EQ(rc, 0) << "scenario run failed for id: " << row.id;

        EXPECT_TRUE(fs::exists(artifactDir / "summary.json"))
            << "missing required artifact summary.json for " << row.id;
        EXPECT_TRUE(fs::exists(artifactDir / "diagnostics.txt"))
            << "missing required artifact diagnostics.txt for " << row.id;
        EXPECT_TRUE(fs::exists(artifactDir / "deterministic_signature.txt"))
            << "missing required artifact deterministic_signature.txt for " << row.id;
    }
}

} // namespace nexus::testing