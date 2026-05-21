#include <nexus/Kernel.h>

#include "editor/EditorSeams.h"

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <vector>

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
using PipeHandle = std::unique_ptr<FILE, PCloseDeleter>;

constexpr std::string_view kUsage =
    "Nexus Tooling App (scaffold)\n"
    "Usage:\n"
    "  nexus_tooling_app [--help]\n"
    "  nexus_tooling_app --list-scenarios [--manifest <path>]\n"
    "  nexus_tooling_app --show-dashboard [--dashboard <path>] [--known-gaps <path>]\n"
    "  nexus_tooling_app --shell [--manifest <path>] [--dashboard <path>]\n"
    "                           [--known-gaps <path>]\n"
    "                           [--scenario <id>] [--artifacts-root <path>]\n"
    "  nexus_tooling_app --show-artifacts <scenario_id> [--artifacts-root <path>]\n"
    "  nexus_tooling_app --run-scenario <scenario_id> [--backend <name>]\n"
    "                           [--runner <path>] [--artifacts-root <path>]\n"
    "  nexus_tooling_app --refresh-dashboard [--dashboard-script <path>] [--skip-gates]\n"
    "                           [--dashboard <path>] [--known-gaps <path>]\n"
    "  nexus_tooling_app --starter-workspace\n"
    "  nexus_tooling_app --starter-model <primitive> [--size <float>] [--cleanup]\n"
    "                           [--export-mesh <path>] [--export-diagnostics <path>]\n"
    "                           [--session-summary]\n"
    "  nexus_tooling_app --import-mesh <path> [--session-summary]\n"
    "  nexus_tooling_app --load-scene <path> [--session-summary]\n"
    "  nexus_tooling_app --editor-seams-demo\n"
    "  nexus_tooling_app --session-panel [--panel-mesh <path>] [--panel-scene <path>]\n"
    "                           [--panel-diagnostics <path>]\n"
    "  nexus_tooling_app --save-session-state --session-kind <kind>\n"
    "                           [--session-label <label>] [--panel-mesh <path>]\n"
    "                           [--panel-scene <path>] [--panel-diagnostics <path>]\n"
    "                           [--state-dir <path>]\n"
    "  nexus_tooling_app --restore-session-state [--state-dir <path>]\n"
    "  nexus_tooling_app --append-state-log <message> [--state-dir <path>]\n"
    "  nexus_tooling_app --show-state-log [--state-dir <path>]\n";

struct ScenarioEntry {
    std::string id;
    std::string title;
    std::string backend;
    std::string script;
    std::string description;
};

std::filesystem::path defaultManifestPath()
{
    return std::filesystem::path{NEXUS_MODELING_SOURCE_ROOT}
           / "scripts/scenarios/manifest.tsv";
}

std::filesystem::path defaultDashboardPath()
{
    return std::filesystem::path{NEXUS_MODELING_SOURCE_ROOT}
           / "build/capability_dashboard.json";
}

std::filesystem::path defaultArtifactsRoot()
{
    return std::filesystem::path{NEXUS_MODELING_SOURCE_ROOT}
           / "build/scenario_artifacts";
}

std::filesystem::path defaultRunScenarioScript()
{
    return std::filesystem::path{NEXUS_MODELING_SOURCE_ROOT}
           / "tools/run_scenario.sh";
}

std::filesystem::path defaultDashboardScript()
{
    return std::filesystem::path{NEXUS_MODELING_SOURCE_ROOT}
           / "tools/capability_dashboard.sh";
}

std::filesystem::path defaultKnownGapsPath()
{
    return std::filesystem::path{NEXUS_MODELING_SOURCE_ROOT}
           / "build/known_gaps.json";
}

// Escape a string for safe inclusion as a single shell-quoted argument.
std::string shellQuote(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2u);
    out += '\'';
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += '\'';
    return out;
}

bool isSafeShellToken(const std::string& token)
{
    if (token.empty()) {
        return false;
    }
    for (char c : token) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                        || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> splitTabSeparated(const std::string& line)
{
    std::vector<std::string> columns;
    size_t start = 0u;

    while (start <= line.size()) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            columns.push_back(line.substr(start));
            break;
        }
        columns.push_back(line.substr(start, tab - start));
        start = tab + 1u;
    }

    return columns;
}

bool loadScenarioManifest(const std::filesystem::path& manifestPath,
                         std::vector<ScenarioEntry>& outEntries,
                         std::string& outError)
{
    outEntries.clear();

    std::ifstream in(manifestPath);
    if (!in.is_open()) {
        outError = "Failed to open scenario manifest: " + manifestPath.string();
        return false;
    }

    std::string line;
    size_t lineNumber = 0u;
    while (std::getline(in, line)) {
        ++lineNumber;

        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::vector<std::string> columns = splitTabSeparated(line);
        if (columns.size() < 5u) {
            outError = "Malformed manifest line " + std::to_string(lineNumber)
                       + ": expected at least 5 tab-separated fields";
            return false;
        }

        std::string description = std::move(columns[4]);
        for (size_t i = 5u; i < columns.size(); ++i) {
            description += '\t';
            description += columns[i];
        }

        ScenarioEntry entry{};
        entry.id = std::move(columns[0]);
        entry.title = std::move(columns[1]);
        entry.backend = std::move(columns[2]);
        entry.script = std::move(columns[3]);
        entry.description = std::move(description);
        outEntries.push_back(std::move(entry));
    }

    return true;
}

std::string trim(std::string value)
{
    const char* whitespace = " \t\n\r";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return {};
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1u);
}

bool loadFileText(const std::filesystem::path& path,
                  std::string& outText,
                  std::string& outError)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        outError = "Failed to open file: " + path.string();
        return false;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    outText = std::move(ss).str();
    return true;
}

std::string extractJsonStringValue(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return {};
    }

    const size_t colon = json.find(':', keyPos + needle.size());
    if (colon == std::string::npos) {
        return {};
    }

    const size_t quoteOpen = json.find('"', colon + 1u);
    if (quoteOpen == std::string::npos) {
        return {};
    }
    const size_t quoteClose = json.find('"', quoteOpen + 1u);
    if (quoteClose == std::string::npos || quoteClose <= quoteOpen) {
        return {};
    }

    return json.substr(quoteOpen + 1u, quoteClose - quoteOpen - 1u);
}

std::string extractJsonStringValueAfterAnchor(const std::string& json,
                                              const std::string& anchor,
                                              const std::string& key)
{
    const std::string anchorNeedle = "\"" + anchor + "\"";
    const size_t anchorPos = json.find(anchorNeedle);
    if (anchorPos == std::string::npos) {
        return {};
    }

    const std::string keyNeedle = "\"" + key + "\"";
    const size_t keyPos = json.find(keyNeedle, anchorPos + anchorNeedle.size());
    if (keyPos == std::string::npos) {
        return {};
    }

    const size_t colon = json.find(':', keyPos + keyNeedle.size());
    if (colon == std::string::npos) {
        return {};
    }

    const size_t quoteOpen = json.find('"', colon + 1u);
    if (quoteOpen == std::string::npos) {
        return {};
    }
    const size_t quoteClose = json.find('"', quoteOpen + 1u);
    if (quoteClose == std::string::npos || quoteClose <= quoteOpen) {
        return {};
    }

    return json.substr(quoteOpen + 1u, quoteClose - quoteOpen - 1u);
}

std::vector<std::pair<std::string, std::string>> extractSubsystemStatuses(const std::string& json)
{
    std::vector<std::pair<std::string, std::string>> result;
    std::istringstream lines(json);

    std::string line;
    std::string pendingName;
    while (std::getline(lines, line)) {
        const std::string t = trim(line);
        if (t.rfind("\"name\"", 0u) == 0u) {
            const size_t firstQuote = t.find('"', 6u);
            if (firstQuote == std::string::npos) {
                continue;
            }
            const size_t secondQuote = t.find('"', firstQuote + 1u);
            if (secondQuote == std::string::npos || secondQuote <= firstQuote) {
                continue;
            }
            pendingName = t.substr(firstQuote + 1u, secondQuote - firstQuote - 1u);
            continue;
        }

        if (!pendingName.empty() && t.rfind("\"status\"", 0u) == 0u) {
            const size_t firstQuote = t.find('"', 8u);
            if (firstQuote == std::string::npos) {
                continue;
            }
            const size_t secondQuote = t.find('"', firstQuote + 1u);
            if (secondQuote == std::string::npos || secondQuote <= firstQuote) {
                continue;
            }
            std::string status = t.substr(firstQuote + 1u, secondQuote - firstQuote - 1u);
            result.emplace_back(std::move(pendingName), std::move(status));
            pendingName.clear();
        }
    }

    return result;
}

struct KnownGap {
    std::string id;
    std::string domain;
    std::string severity;
    std::string status;
    std::string userImpact;
};

struct KnownGapsReport {
    std::filesystem::path path;
    bool fileFound = false;
    bool parseError = false;
    std::string parseMessage;
    std::vector<KnownGap> entries;
};

// Extract a "key": "value" pair from a fragment of JSON object text.
std::string extractFieldFromObject(const std::string& object, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    const size_t keyPos = object.find(needle);
    if (keyPos == std::string::npos) {
        return {};
    }
    const size_t colon = object.find(':', keyPos + needle.size());
    if (colon == std::string::npos) {
        return {};
    }
    const size_t quoteOpen = object.find('"', colon + 1u);
    if (quoteOpen == std::string::npos) {
        return {};
    }
    // Honor minimal backslash escaping (sufficient for known_gaps.json shape).
    std::string out;
    for (size_t i = quoteOpen + 1u; i < object.size(); ++i) {
        const char c = object[i];
        if (c == '\\' && i + 1u < object.size()) {
            out += object[++i];
            continue;
        }
        if (c == '"') {
            return out;
        }
        out += c;
    }
    return {};
}

KnownGapsReport loadKnownGaps(const std::filesystem::path& path)
{
    KnownGapsReport report{};
    report.path = path;
    if (!std::filesystem::exists(path)) {
        return report;
    }
    report.fileFound = true;

    std::string text;
    std::string err;
    if (!loadFileText(path, text, err)) {
        report.parseError = true;
        report.parseMessage = err;
        return report;
    }

    // Walk top-level JSON array, slicing each object by tracking brace depth.
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '{') {
            ++i;
            continue;
        }
        size_t depth = 0;
        bool inString = false;
        bool escape = false;
        size_t start = i;
        for (; i < text.size(); ++i) {
            const char c = text[i];
            if (escape) {
                escape = false;
                continue;
            }
            if (inString) {
                if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    inString = false;
                }
                continue;
            }
            if (c == '"') {
                inString = true;
            } else if (c == '{') {
                ++depth;
            } else if (c == '}') {
                --depth;
                if (depth == 0) {
                    const std::string obj = text.substr(start, i - start + 1u);
                    KnownGap g;
                    g.id = extractFieldFromObject(obj, "id");
                    g.domain = extractFieldFromObject(obj, "domain");
                    g.severity = extractFieldFromObject(obj, "severity");
                    g.status = extractFieldFromObject(obj, "status");
                    g.userImpact = extractFieldFromObject(obj, "user_impact");
                    if (!g.id.empty() || !g.domain.empty() || !g.userImpact.empty()) {
                        report.entries.push_back(std::move(g));
                    }
                    ++i;
                    break;
                }
            }
        }
    }
    return report;
}

struct ScenarioArtifacts {
    std::string scenarioId;
    std::filesystem::path directory;
    bool directoryExists = false;

    bool summaryFound = false;
    std::string summaryStatus;
    std::string summaryBackend;
    std::string summaryTestFilter;
    std::string summaryError;

    bool diagnosticsFound = false;
    std::uintmax_t diagnosticsSize = 0u;
    std::size_t diagnosticsLineCount = 0u;
    std::vector<std::string> diagnosticsHead;

    bool framePngFound = false;
    std::uintmax_t framePngSize = 0u;

    bool frameHashFound = false;
    std::string frameHash;
};

ScenarioArtifacts loadScenarioArtifacts(const std::filesystem::path& artifactsRoot,
                                        const std::string& scenarioId)
{
    ScenarioArtifacts out{};
    out.scenarioId = scenarioId;
    out.directory = artifactsRoot / scenarioId;
    out.directoryExists = std::filesystem::is_directory(out.directory);

    if (!out.directoryExists) {
        return out;
    }

    const auto summaryPath = out.directory / "summary.json";
    if (std::filesystem::is_regular_file(summaryPath)) {
        std::string text;
        std::string err;
        if (loadFileText(summaryPath, text, err)) {
            out.summaryFound = true;
            out.summaryStatus = extractJsonStringValue(text, "status");
            out.summaryBackend = extractJsonStringValue(text, "backend");
            out.summaryTestFilter = extractJsonStringValue(text, "test_filter");
        } else {
            out.summaryError = err;
        }
    }

    const auto diagPath = out.directory / "diagnostics.txt";
    if (std::filesystem::is_regular_file(diagPath)) {
        out.diagnosticsFound = true;
        std::error_code ec;
        out.diagnosticsSize = std::filesystem::file_size(diagPath, ec);
        std::ifstream in(diagPath);
        std::string line;
        while (std::getline(in, line)) {
            ++out.diagnosticsLineCount;
            if (out.diagnosticsHead.size() < 5u) {
                out.diagnosticsHead.push_back(std::move(line));
            }
        }
    }

    const auto framePngPath = out.directory / "frame.png";
    if (std::filesystem::is_regular_file(framePngPath)) {
        out.framePngFound = true;
        std::error_code ec;
        out.framePngSize = std::filesystem::file_size(framePngPath, ec);
    }

    const auto frameHashPath = out.directory / "frame_hash.txt";
    if (std::filesystem::is_regular_file(frameHashPath)) {
        out.frameHashFound = true;
        std::string text;
        std::string err;
        if (loadFileText(frameHashPath, text, err)) {
            out.frameHash = trim(std::move(text));
        }
    }

    return out;
}

} // namespace

void printArtifactReport(const ScenarioArtifacts& a)
{
    std::cout << "Scenario artifacts: " << a.scenarioId << "\n";
    std::cout << "  directory: " << a.directory.string() << "\n";

    if (!a.directoryExists) {
        std::cout << "  status: artifacts directory not found\n";
        std::cout << "  hint: run tools/run_scenario.sh " << a.scenarioId << "\n";
        return;
    }

    // summary.json
    if (!a.summaryFound) {
        std::cout << "  summary.json: missing (empty-state)\n";
    } else if (!a.summaryError.empty()) {
        std::cout << "  summary.json: error - " << a.summaryError << "\n";
    } else {
        std::cout << "  summary.json: status=" << (a.summaryStatus.empty() ? "(unknown)" : a.summaryStatus)
                  << " backend=" << (a.summaryBackend.empty() ? "(unknown)" : a.summaryBackend) << "\n";
        if (!a.summaryTestFilter.empty()) {
            std::cout << "    test_filter: " << a.summaryTestFilter << "\n";
        }
        if (a.summaryStatus == "fail") {
            std::cout << "  *** SCENARIO STATUS: FAIL ***\n";
        }
    }

    // diagnostics.txt
    if (!a.diagnosticsFound) {
        std::cout << "  diagnostics.txt: missing (empty-state)\n";
    } else {
        std::cout << "  diagnostics.txt: " << a.diagnosticsLineCount << " lines, "
                  << a.diagnosticsSize << " bytes\n";
        for (const std::string& line : a.diagnosticsHead) {
            std::cout << "    | " << line << "\n";
        }
        if (a.diagnosticsLineCount > a.diagnosticsHead.size()) {
            std::cout << "    | ... (" << (a.diagnosticsLineCount - a.diagnosticsHead.size())
                      << " more lines)\n";
        }
    }

    // frame.png and frame_hash.txt
    if (!a.framePngFound && !a.frameHashFound) {
        std::cout << "  frame.png / frame_hash.txt: missing (empty-state - scenario has no frame artifact)\n";
    } else {
        if (a.framePngFound) {
            std::cout << "  frame.png: present (" << a.framePngSize << " bytes)\n";
        } else {
            std::cout << "  frame.png: missing\n";
        }
        if (a.frameHashFound) {
            std::cout << "  frame_hash.txt: " << a.frameHash << "\n";
        } else {
            std::cout << "  frame_hash.txt: missing\n";
        }
    }
}

int runScenarioCommand(const std::filesystem::path& runnerScript,
                       const std::string& scenarioId,
                       const std::string& backendOverride,
                       const std::filesystem::path& artifactsRoot)
{
    if (!isSafeShellToken(scenarioId)) {
        std::cerr << "Refusing to run scenario: id contains unsafe characters: "
                  << scenarioId << "\n";
        return 2;
    }
    if (!backendOverride.empty() && !isSafeShellToken(backendOverride)) {
        std::cerr << "Refusing to run scenario: backend contains unsafe characters: "
                  << backendOverride << "\n";
        return 2;
    }
    if (!std::filesystem::is_regular_file(runnerScript)) {
        std::cerr << "Scenario runner not found: " << runnerScript.string() << "\n";
        return 2;
    }

    std::cout << "Running scenario via " << runnerScript.string()
              << " " << scenarioId;
    if (!backendOverride.empty()) {
        std::cout << " " << backendOverride;
    }
    std::cout << "\n";
    std::cout << "----------------------------------------------------------------\n";

    std::string command = runnerScript.string();
    command += ' ';
    command += scenarioId;
    if (!backendOverride.empty()) {
        command += ' ';
        command += backendOverride;
    }
    command += " 2>&1";

    PipeHandle pipe(popen(command.c_str(), "r"));
    if (!pipe) {
        std::cerr << "Failed to launch scenario runner\n";
        return 1;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        std::cout << buffer;
    }

    int rawStatus = pclose(pipe.release());
    const int exitCode = WIFEXITED(rawStatus) ? WEXITSTATUS(rawStatus) : 1;

    std::cout << "----------------------------------------------------------------\n";
    std::cout << "Scenario runner exit code: " << exitCode << "\n";
    std::cout << "Refreshed artifacts:\n";

    const ScenarioArtifacts refreshed = loadScenarioArtifacts(artifactsRoot, scenarioId);
    printArtifactReport(refreshed);

    if (exitCode != 0) {
        return exitCode;
    }
    if (refreshed.summaryFound && refreshed.summaryStatus == "fail") {
        return 1;
    }
    return 0;
}

void printKnownGapsReport(const KnownGapsReport& report)
{
    if (!report.fileFound) {
        std::cout << "Known gaps: " << report.path.string() << "\n";
        std::cout << "  (file not found - run tools/capability_dashboard.sh to generate it)\n";
        return;
    }
    if (report.parseError) {
        std::cout << "Known gaps: " << report.path.string() << "\n";
        std::cout << "  (" << report.parseMessage << ")\n";
        return;
    }

    std::cout << "Known gaps: " << report.path.string() << "\n";
    if (report.entries.empty()) {
        std::cout << "  (no open gaps - clean state)\n";
        return;
    }
    std::cout << "  " << report.entries.size() << " gap(s):\n";
    for (const KnownGap& g : report.entries) {
        std::cout << "    - [" << g.severity << "/" << g.status << "] "
                  << g.id;
        if (!g.domain.empty()) {
            std::cout << " (" << g.domain << ")";
        }
        std::cout << "\n";
        if (!g.userImpact.empty()) {
            std::cout << "        impact: " << g.userImpact << "\n";
        }
    }
}

int refreshDashboardCommand(const std::filesystem::path& dashboardScript,
                            const std::filesystem::path& dashboardPath,
                            const std::filesystem::path& knownGapsPath,
                            bool skipGates)
{
    if (!std::filesystem::is_regular_file(dashboardScript)) {
        std::cerr << "Dashboard script not found: " << dashboardScript.string() << "\n";
        return 2;
    }

    std::cout << "Refreshing dashboard via " << dashboardScript.string()
              << (skipGates ? " (gates skipped)" : "") << "\n";
    std::cout << "----------------------------------------------------------------\n";

    std::string command = shellQuote(dashboardScript.string());
    command += " --output ";
    command += shellQuote(dashboardPath.string());
    command += " --known-gaps-output ";
    command += shellQuote(knownGapsPath.string());
    if (skipGates) {
        command += " --skip-gates";
    }
    command += " 2>&1";

    PipeHandle pipe(popen(command.c_str(), "r"));
    if (!pipe) {
        std::cerr << "Failed to launch dashboard script\n";
        return 1;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        std::cout << buffer;
    }

    const int rawStatus = pclose(pipe.release());
    const int exitCode = WIFEXITED(rawStatus) ? WEXITSTATUS(rawStatus) : 1;

    std::cout << "----------------------------------------------------------------\n";
    std::cout << "Dashboard script exit code: " << exitCode << "\n";

    // Surface refreshed dashboard summary, even on partial failures, so the
    // app reflects whatever state the script left on disk.
    if (std::filesystem::exists(dashboardPath)) {
        std::string json;
        std::string err;
        if (loadFileText(dashboardPath, json, err)) {
            const std::string generatedAt = extractJsonStringValue(json, "generated_at_utc");
            const std::string deterministicStatus =
                extractJsonStringValueAfterAnchor(json, "deterministic_checks", "status");
            std::cout << "Refreshed dashboard: " << dashboardPath.string() << "\n";
            if (!generatedAt.empty()) {
                std::cout << "  generated_at_utc: " << generatedAt << "\n";
            }
            if (!deterministicStatus.empty()) {
                std::cout << "  deterministic_checks.status: " << deterministicStatus << "\n";
            }
            const auto subsystemStatuses = extractSubsystemStatuses(json);
            if (!subsystemStatuses.empty()) {
                std::cout << "  subsystems:\n";
                for (const auto& [name, status] : subsystemStatuses) {
                    std::cout << "    - " << name << ": " << status << "\n";
                }
            }
        } else {
            std::cout << "Refreshed dashboard: " << dashboardPath.string() << "\n";
            std::cout << "  (" << err << ")\n";
        }
    } else {
        std::cout << "Refreshed dashboard: " << dashboardPath.string() << "\n";
        std::cout << "  (file not written - script may have failed before output)\n";
    }

    printKnownGapsReport(loadKnownGaps(knownGapsPath));

    return exitCode;
}

void renderShellLayout(const std::filesystem::path& manifestPath,
                       const std::filesystem::path& dashboardPath,
                       const std::filesystem::path& knownGapsPath,
                       const std::filesystem::path& artifactsRoot,
                       const std::string& selectedScenarioId)
{
    std::cout << "================================================================\n";
    std::cout << " Nexus Tooling App - Viewer Shell (Week 2 Day 1, static layout) \n";
    std::cout << "================================================================\n";

    // Pane 1: Scenario list
    std::cout << "[1] Scenario List\n";
    std::cout << "----------------------------------------------------------------\n";
    std::vector<ScenarioEntry> entries;
    std::string manifestError;
    if (loadScenarioManifest(manifestPath, entries, manifestError)) {
        if (entries.empty()) {
            std::cout << "    (manifest empty)\n";
        } else {
            for (const ScenarioEntry& s : entries) {
                std::cout << "    - " << s.id << " [" << s.backend << "]\n";
            }
        }
    } else {
        std::cout << "    (" << manifestError << ")\n";
    }

    // Pane 2: Viewport / image panel
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "[2] Viewport / Image Panel\n";
    std::cout << "----------------------------------------------------------------\n";
    if (selectedScenarioId.empty()) {
        std::cout << "    (no scenario selected - pass --scenario <id> to load artifacts)\n";
    } else {
        const ScenarioArtifacts a = loadScenarioArtifacts(artifactsRoot, selectedScenarioId);
        if (!a.directoryExists) {
            std::cout << "    (artifacts directory missing for " << selectedScenarioId << ")\n";
        } else if (a.framePngFound) {
            std::cout << "    frame.png present (" << a.framePngSize << " bytes) at "
                      << (a.directory / "frame.png").string() << "\n";
        } else if (a.frameHashFound) {
            std::cout << "    frame_hash: " << a.frameHash << "\n";
            std::cout << "    (no frame.png - scenario presents a deterministic hash instead)\n";
        } else {
            std::cout << "    (no frame artifact - scenario does not produce a frame)\n";
        }
    }

    // Pane 3: Diagnostics panel
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "[3] Diagnostics Panel\n";
    std::cout << "----------------------------------------------------------------\n";
    if (selectedScenarioId.empty()) {
        std::cout << "    (no scenario selected - pass --scenario <id> to load diagnostics)\n";
    } else {
        const ScenarioArtifacts a = loadScenarioArtifacts(artifactsRoot, selectedScenarioId);
        if (!a.directoryExists) {
            std::cout << "    (artifacts directory missing for " << selectedScenarioId << ")\n";
        } else if (!a.diagnosticsFound) {
            std::cout << "    (diagnostics.txt missing - empty-state)\n";
        } else {
            std::cout << "    " << a.diagnosticsLineCount << " lines, "
                      << a.diagnosticsSize << " bytes\n";
            for (const std::string& line : a.diagnosticsHead) {
                std::cout << "    | " << line << "\n";
            }
            if (a.diagnosticsLineCount > a.diagnosticsHead.size()) {
                std::cout << "    | ... (" << (a.diagnosticsLineCount - a.diagnosticsHead.size())
                          << " more lines)\n";
            }
            if (a.summaryFound && a.summaryStatus == "fail") {
                std::cout << "    *** scenario status: FAIL ***\n";
            }
        }
    }

    // Pane 4: Subsystem status panel (dashboard summary)
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "[4] Subsystem Status Panel\n";
    std::cout << "----------------------------------------------------------------\n";
    if (!std::filesystem::exists(dashboardPath)) {
        std::cout << "    (dashboard not found - run tools/capability_dashboard.sh)\n";
    } else {
        std::string json;
        std::string err;
        if (!loadFileText(dashboardPath, json, err)) {
            std::cout << "    (" << err << ")\n";
        } else {
            const auto subsystemStatuses = extractSubsystemStatuses(json);
            if (subsystemStatuses.empty()) {
                std::cout << "    (dashboard format unrecognized)\n";
            } else {
                for (const auto& [name, status] : subsystemStatuses) {
                    std::cout << "    - " << name << ": " << status << "\n";
                }
            }
        }
    }

    // Pane 4b: Known gaps (Week 2 Day 4)
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "[4b] Known Gaps\n";
    std::cout << "----------------------------------------------------------------\n";
    {
        const KnownGapsReport gaps = loadKnownGaps(knownGapsPath);
        if (!gaps.fileFound) {
            std::cout << "    (known_gaps.json not found - run tools/capability_dashboard.sh"
                         " or --refresh-dashboard)\n";
        } else if (gaps.parseError) {
            std::cout << "    (" << gaps.parseMessage << ")\n";
        } else if (gaps.entries.empty()) {
            std::cout << "    (no open gaps - clean state)\n";
        } else {
            for (const KnownGap& g : gaps.entries) {
                std::cout << "    - [" << g.severity << "/" << g.status << "] "
                          << g.id;
                if (!g.domain.empty()) {
                    std::cout << " (" << g.domain << ")";
                }
                std::cout << "\n";
            }
        }
    }

    // Pane 5: Run controls
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "[5] Run Controls\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "    use --run-scenario <id> to execute a scenario,\n";
    std::cout << "    or --refresh-dashboard to regenerate dashboard + known gaps\n";

    // Pane 6: Starter Workspace (Week 3 PR-11 — choice surface only, no execution).
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "[6] Starter Workspace\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "    Primitives: Box, Plane, Sphere, Cylinder, Cone, Capsule\n";
    std::cout << "    Run --starter-workspace for guided intro.\n";
    std::cout << "    (advanced modeling panels hidden by design)\n";
    std::cout << "================================================================\n";
}

void printStarterWorkspace()
{
    // Day 1 of Week 3: present the primitive choice surface only.
    // No startStarterModel / quickCleanup / overlay invocations yet — those land in PR-12+.
    std::cout << "Starter modeling workspace\n";
    std::cout << "================================================================\n";

    std::cout << "Guided intro:\n";
    const auto steps = nexus::geometry::ModelingShell::guidedIntroSteps();
    for (size_t i = 0; i < steps.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << steps[i] << "\n";
    }

    std::cout << "\nAvailable starter primitives:\n";
    // Mirrors nexus::geometry::StarterPrimitive enum order; app-side label only.
    static constexpr const char* kPrimitives[] = {
        "Box", "Plane", "Sphere", "Cylinder", "Cone", "Capsule",
    };
    for (const char* name : kPrimitives) {
        std::cout << "  - " << name << "\n";
    }

    std::cout << "\nAdvanced modeling panels are hidden by design (simple-by-default).\n";
    std::cout << "Next step: pick one and run --starter-model <primitive> [--size <float>].\n";
}

bool parseStarterPrimitive(std::string_view name, nexus::geometry::StarterPrimitive& out)
{
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    using P = nexus::geometry::StarterPrimitive;
    if (lower == "box")      { out = P::Box; return true; }
    if (lower == "plane")    { out = P::Plane; return true; }
    if (lower == "sphere")   { out = P::Sphere; return true; }
    if (lower == "cylinder") { out = P::Cylinder; return true; }
    if (lower == "cone")     { out = P::Cone; return true; }
    if (lower == "capsule")  { out = P::Capsule; return true; }
    return false;
}

std::string_view stepKindLabel(nexus::geometry::HardSurfaceStepKind kind)
{
    using K = nexus::geometry::HardSurfaceStepKind;
    switch (kind) {
    case K::Init:             return "Init";
    case K::ApplyTransform:   return "ApplyTransform";
    case K::BooleanOperation: return "BooleanOperation";
    case K::Triangulate:      return "Triangulate";
    case K::BevelChamfer:     return "BevelChamfer";
    case K::Remesh:           return "Remesh";
    case K::RebuildNormals:   return "RebuildNormals";
    }
    return "Unknown";
}

// PR-15: tiny in-process session model. The CLI is per-invocation, so this
// aggregate is built up inside one subcommand and (optionally) printed as a
// consolidated footer via --session-summary. It deliberately mirrors only the
// public surfaces named in the Week 3 plan:
//   - current scenario or starter-model session
//   - current artifact paths
//   - current diagnostics snapshot
//   - current model state handle (kept as an app-side readiness flag, not a
//     pointer into the kernel)
struct SessionModel {
    enum class Kind { None, StarterModel, ImportMesh, LoadScene, Scenario };
    Kind kind{Kind::None};
    std::string label;
    bool ready{false};
    bool sessionSuccess{false};
    std::size_t workflowStepCount{0};
    bool diagnosticsValid{false};
    nexus::geometry::MeshDiagnosticOverlayData diagnostics{};
    std::vector<std::filesystem::path> artifactPaths;
};

const char* sessionKindLabel(SessionModel::Kind k)
{
    switch (k) {
        case SessionModel::Kind::StarterModel: return "starter-model";
        case SessionModel::Kind::ImportMesh:   return "import-mesh";
        case SessionModel::Kind::LoadScene:    return "load-scene";
        case SessionModel::Kind::Scenario:     return "scenario";
        case SessionModel::Kind::None:
        default:                               return "none";
    }
}

void printSessionSummary(const SessionModel& session)
{
    std::cout << "session summary:\n";
    std::cout << "  kind: " << sessionKindLabel(session.kind) << "\n";
    std::cout << "  label: " << session.label << "\n";
    std::cout << "  ready: " << (session.ready ? "true" : "false") << "\n";
    std::cout << "  session_success: " << (session.sessionSuccess ? "true" : "false") << "\n";
    std::cout << "  workflow_step_count: " << session.workflowStepCount << "\n";
    std::cout << "  diagnostics_valid: " << (session.diagnosticsValid ? "true" : "false") << "\n";
    std::cout << "  diagnostics.non_manifold_edges: " << session.diagnostics.nonManifoldEdgeCount << "\n";
    std::cout << "  diagnostics.boundary_edges: "    << session.diagnostics.boundaryEdgeCount << "\n";
    std::cout << "  diagnostics.degenerate_faces: "  << session.diagnostics.degenerateFaceCount << "\n";
    std::cout << "  diagnostics.isolated_vertices: " << session.diagnostics.isolatedVertexCount << "\n";
    std::cout << "  artifact_count: " << session.artifactPaths.size() << "\n";
    for (const auto& p : session.artifactPaths) {
        std::cout << "    - " << p.string() << "\n";
    }
}

void printOverlaySummary(const nexus::geometry::MeshDiagnosticOverlayData& overlay,
                         bool diagnosticsOk,
                         std::string_view label)
{
    std::cout << "  diagnostics overlay [" << label << "] (refresh: "
              << (diagnosticsOk ? "ok" : "unavailable") << "):\n";
    std::cout << "    edges: "             << overlay.edges.size() << "\n";
    std::cout << "    faces: "             << overlay.faces.size() << "\n";
    std::cout << "    vertices: "          << overlay.vertices.size() << "\n";
    std::cout << "    non_manifold_edges: " << overlay.nonManifoldEdgeCount << "\n";
    std::cout << "    boundary_edges: "    << overlay.boundaryEdgeCount << "\n";
    std::cout << "    degenerate_faces: "  << overlay.degenerateFaceCount << "\n";
    std::cout << "    isolated_vertices: " << overlay.isolatedVertexCount << "\n";
}

int runStarterModel(std::string_view primitiveName,
                    float size,
                    bool applyCleanup,
                    const std::filesystem::path& exportMeshPath,
                    const std::filesystem::path& exportDiagnosticsPath,
                    bool emitSessionSummary)
{
    nexus::geometry::StarterPrimitive primitive{};
    if (!parseStarterPrimitive(primitiveName, primitive)) {
        std::cerr << "Unknown starter primitive: " << primitiveName << "\n";
        std::cerr << "Valid choices: Box, Plane, Sphere, Cylinder, Cone, Capsule\n";
        return 2;
    }

    nexus::geometry::ModelingShell shell;
    shell.startStarterModel(primitive, size);
    const bool startupDiagnosticsOk = shell.refreshDiagnostics();

    std::cout << "Starter model session\n";
    std::cout << "================================================================\n";
    std::cout << "  primitive: " << primitiveName << "\n";
    std::cout << "  size: " << size << "\n";
    std::cout << "  cleanup_requested: " << (applyCleanup ? "true" : "false") << "\n";

    // Pre-cleanup snapshot — always printed so the user sees the starter state
    // before any cleanup mutation. Snapshot reads via public diagnostics() accessor.
    printOverlaySummary(shell.diagnostics(), startupDiagnosticsOk, "pre-cleanup");

    bool cleanupDiagnosticsOk = startupDiagnosticsOk;
    if (applyCleanup) {
        // quickCleanup() runs BevelChamfer + Remesh + RebuildNormals and internally
        // refreshes diagnostics. The session report below will surface the new steps.
        shell.quickCleanup();
        // Re-fetch to make the refreshed status observable to the caller.
        cleanupDiagnosticsOk = shell.refreshDiagnostics();
        printOverlaySummary(shell.diagnostics(), cleanupDiagnosticsOk, "post-cleanup");
    }

    const auto report = shell.sessionReport();

    std::cout << "  ready: " << (shell.isReady() ? "true" : "false") << "\n";
    std::cout << "  session.success: " << (report.success ? "true" : "false") << "\n";
    if (!report.message.empty()) {
        std::cout << "  session.message: " << report.message << "\n";
    }

    std::cout << "  workflow steps (" << report.workflowSteps.size() << "):\n";
    for (const auto& step : report.workflowSteps) {
        std::cout << "    - " << stepKindLabel(step.kind)
                  << ": " << (step.success ? "ok" : "fail");
        if (!step.message.empty()) {
            std::cout << " (" << step.message << ")";
        }
        std::cout << "\n";
    }

    int exportFailures = 0;
    if (!exportMeshPath.empty()) {
        nexus::geometry::MeshExportOptions options{};
        const std::string ext = exportMeshPath.extension().string();
        std::string lowerExt;
        lowerExt.reserve(ext.size());
        for (char c : ext) {
            lowerExt.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        if (lowerExt == ".ply") {
            options.format = nexus::geometry::MeshExportFormat::PLY;
        } else {
            options.format = nexus::geometry::MeshExportFormat::OBJ;
        }

        const auto exportReport = nexus::geometry::MeshIO::exportMesh(
            shell.workflow().mesh(), exportMeshPath.string(), options);
        std::cout << "  mesh export:\n";
        std::cout << "    path: " << exportMeshPath.string() << "\n";
        std::cout << "    format: " << (options.format == nexus::geometry::MeshExportFormat::PLY ? "PLY" : "OBJ") << "\n";
        std::cout << "    success: " << (exportReport.isSuccess() ? "true" : "false") << "\n";
        std::cout << "    vertices_written: " << exportReport.verticesWritten << "\n";
        std::cout << "    faces_written: " << exportReport.facesWritten << "\n";
        for (const auto& msg : exportReport.messages) {
            std::cout << "    message: " << msg << "\n";
        }
        if (!exportReport.isSuccess()) {
            ++exportFailures;
        }
    }

    if (!exportDiagnosticsPath.empty()) {
        std::ofstream out(exportDiagnosticsPath);
        if (!out.is_open()) {
            std::cerr << "Failed to open diagnostics export path: "
                      << exportDiagnosticsPath.string() << "\n";
            ++exportFailures;
        } else {
            const auto& overlay = shell.diagnostics();
            out << "primitive: " << primitiveName << "\n";
            out << "size: " << size << "\n";
            out << "cleanup_applied: " << (applyCleanup ? "true" : "false") << "\n";
            out << "session_success: " << (report.success ? "true" : "false") << "\n";
            out << "workflow_step_count: " << report.workflowSteps.size() << "\n";
            out << "overlay_edges: "             << overlay.edges.size() << "\n";
            out << "overlay_faces: "             << overlay.faces.size() << "\n";
            out << "overlay_vertices: "          << overlay.vertices.size() << "\n";
            out << "non_manifold_edges: "        << overlay.nonManifoldEdgeCount << "\n";
            out << "boundary_edges: "            << overlay.boundaryEdgeCount << "\n";
            out << "degenerate_faces: "          << overlay.degenerateFaceCount << "\n";
            out << "isolated_vertices: "         << overlay.isolatedVertexCount << "\n";
            std::cout << "  diagnostics export:\n";
            std::cout << "    path: " << exportDiagnosticsPath.string() << "\n";
            std::cout << "    success: true\n";
        }
    }

    if (emitSessionSummary) {
        SessionModel session;
        session.kind = SessionModel::Kind::StarterModel;
        session.label = std::string{primitiveName};
        session.ready = shell.isReady();
        session.sessionSuccess = report.success;
        session.workflowStepCount = report.workflowSteps.size();
        session.diagnosticsValid = cleanupDiagnosticsOk;
        session.diagnostics = shell.diagnostics();
        if (!exportMeshPath.empty())        session.artifactPaths.push_back(exportMeshPath);
        if (!exportDiagnosticsPath.empty()) session.artifactPaths.push_back(exportDiagnosticsPath);
        printSessionSummary(session);
    }

    if (exportFailures > 0) {
        return 1;
    }
    return report.success ? 0 : 1;
}

int runImportMesh(const std::filesystem::path& path, bool emitSessionSummary)
{
    if (path.empty()) {
        std::cerr << "Missing path for --import-mesh\n";
        return 2;
    }
    nexus::geometry::Mesh mesh;
    nexus::geometry::MeshImportOptions options{};
    const auto report = nexus::geometry::MeshIO::importMesh(path.string(), mesh, options);

    std::cout << "Mesh import\n";
    std::cout << "================================================================\n";
    std::cout << "  path: " << path.string() << "\n";
    std::cout << "  format: OBJ\n";
    std::cout << "  success: " << (report.isSuccess() ? "true" : "false") << "\n";
    std::cout << "  valid: " << (report.valid ? "true" : "false") << "\n";
    std::cout << "  vertices_read: " << report.verticesRead << "\n";
    std::cout << "  faces_read: " << report.facesRead << "\n";
    for (const auto& msg : report.messages) {
        std::cout << "  message: " << msg << "\n";
    }
    if (emitSessionSummary) {
        SessionModel session;
        session.kind = SessionModel::Kind::ImportMesh;
        session.label = path.string();
        session.ready = report.isSuccess() && report.valid;
        session.sessionSuccess = report.isSuccess();
        session.workflowStepCount = 0;
        session.diagnosticsValid = false;
        session.artifactPaths.push_back(path);
        printSessionSummary(session);
    }
    return report.isSuccess() ? 0 : 1;
}

void printEditorSeamState(const nexus::tooling::editor::EditorDocument& doc,
                          const nexus::tooling::editor::SelectionModel& sel,
                          const nexus::tooling::editor::CommandStack& stack,
                          std::string_view step)
{
    std::cout << "  [" << step << "]\n";
    std::cout << "    document.label: " << doc.label() << "\n";
    std::cout << "    document.session_kind: " << doc.sessionKind() << "\n";
    std::cout << "    document.dirty: " << (doc.isDirty() ? "true" : "false") << "\n";
    std::cout << "    selection.size: " << sel.size() << "\n";
    const auto ids = sel.ids();
    for (const auto& id : ids) {
        std::cout << "      - " << id << "\n";
    }
    std::cout << "    undo_depth: " << stack.undoDepth() << "\n";
    std::cout << "    redo_depth: " << stack.redoDepth() << "\n";
    std::cout << "    can_undo: " << (stack.canUndo() ? "true" : "false") << "\n";
    std::cout << "    can_redo: " << (stack.canRedo() ? "true" : "false") << "\n";
}

int runEditorSeamsDemo()
{
    using namespace nexus::tooling::editor;

    EditorDocument doc;
    SelectionModel sel;
    CommandStack stack;

    std::cout << "Editor seams demo\n";
    std::cout << "================================================================\n";
    std::cout << "  max_undo_depth: " << stack.maxDepth() << "\n";
    printEditorSeamState(doc, sel, stack, "initial");

    doc.setSessionKind("starter-model");
    if (!stack.execute(std::make_unique<SetDocumentLabelCommand>("starter-cube"), doc, sel)) {
        std::cerr << "SetDocumentLabelCommand failed\n";
        return 1;
    }
    printEditorSeamState(doc, sel, stack, "after_set_label");

    if (!stack.execute(std::make_unique<AddSelectionCommand>(
                           std::vector<std::string>{"v0", "v1", "v2"}),
                       doc, sel)) {
        std::cerr << "AddSelectionCommand failed\n";
        return 1;
    }
    printEditorSeamState(doc, sel, stack, "after_add_selection");

    if (!stack.undo(doc, sel)) {
        std::cerr << "undo failed\n";
        return 1;
    }
    printEditorSeamState(doc, sel, stack, "after_undo_selection");

    if (!stack.undo(doc, sel)) {
        std::cerr << "undo failed\n";
        return 1;
    }
    printEditorSeamState(doc, sel, stack, "after_undo_label");

    if (!stack.redo(doc, sel)) {
        std::cerr << "redo failed\n";
        return 1;
    }
    printEditorSeamState(doc, sel, stack, "after_redo_label");

    // Demonstrate that pushing a new command after a partial undo clears redo.
    if (!stack.execute(std::make_unique<AddSelectionCommand>(
                           std::vector<std::string>{"e0"}),
                       doc, sel)) {
        std::cerr << "AddSelectionCommand (branch) failed\n";
        return 1;
    }
    printEditorSeamState(doc, sel, stack, "after_branch_new_command");

    return 0;
}

// PR-17: read-only session/scene panel. Composes a small four-pane text layout
// from optional on-disk artifacts (mesh, scene, diagnostics). Designed for
// orientation only — no editing behavior, no kernel state mutation.
struct PanelDiagnosticsSnapshot {
    bool                         present = false;
    bool                         readSucceeded = false;
    std::filesystem::path        source;
    std::optional<unsigned long> nonManifoldEdges;
    std::optional<unsigned long> boundaryEdges;
    std::optional<unsigned long> degenerateFaces;
    std::optional<unsigned long> isolatedVertices;
};

bool parsePanelDiagnosticsFile(const std::filesystem::path& path,
                               PanelDiagnosticsSnapshot& snapshot)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // Trim leading spaces from value.
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.erase(value.begin());
        }
        auto toUL = [&](const std::string& v) -> std::optional<unsigned long> {
            try { return std::stoul(v); } catch (...) { return std::nullopt; }
        };
        if (key == "non_manifold_edges")  snapshot.nonManifoldEdges  = toUL(value);
        else if (key == "boundary_edges")    snapshot.boundaryEdges    = toUL(value);
        else if (key == "degenerate_faces")  snapshot.degenerateFaces  = toUL(value);
        else if (key == "isolated_vertices") snapshot.isolatedVertices = toUL(value);
    }
    return true;
}

int runSessionPanel(const std::filesystem::path& meshPath,
                    const std::filesystem::path& scenePath,
                    const std::filesystem::path& diagnosticsPath)
{
    // [document] pane: derive label/kind purely from which inputs were supplied.
    std::string documentLabel = "untitled";
    std::string sessionKind   = "none";

    // [scene outline] pane state.
    bool   sceneProvided = !scenePath.empty();
    bool   sceneOk       = false;
    bool   sceneValid    = false;
    std::string sceneName;
    std::vector<std::string> sceneEntryLines;
    if (sceneProvided) {
        nexus::asset::SceneAsset scene;
        const auto report = scene.load(scenePath.string());
        sceneOk    = report.isSuccess();
        sceneValid = report.valid;
        sceneName  = scene.sceneName();
        for (size_t i = 0; i < scene.entryCount(); ++i) {
            const auto& e = scene.entry(i);
            std::string line = e.name.empty() ? std::string{"(unnamed)"} : e.name;
            line += e.visible ? " (visible=true" : " (visible=false";
            if (!e.sourcePath.empty()) { line += ", source="; line += e.sourcePath; }
            line += ")";
            sceneEntryLines.push_back(std::move(line));
        }
        if (sceneOk && !sceneName.empty()) documentLabel = sceneName;
        sessionKind = "scene";
    }

    // [workflow status] pane state.
    bool meshProvided = !meshPath.empty();
    bool meshOk = false;
    bool meshValid = false;
    size_t meshVertices = 0;
    size_t meshFaces = 0;
    if (meshProvided) {
        nexus::geometry::Mesh mesh;
        const auto report = nexus::geometry::MeshIO::importMesh(meshPath.string(), mesh, {});
        meshOk    = report.isSuccess();
        meshValid = report.valid;
        meshVertices = report.verticesRead;
        meshFaces    = report.facesRead;
        if (!sceneProvided) {
            documentLabel = meshPath.stem().string();
            if (documentLabel.empty()) documentLabel = meshPath.string();
            sessionKind = "mesh";
        }
    }

    // [diagnostics summary] pane state.
    PanelDiagnosticsSnapshot diag;
    if (!diagnosticsPath.empty()) {
        diag.present = true;
        diag.source = diagnosticsPath;
        diag.readSucceeded = parsePanelDiagnosticsFile(diagnosticsPath, diag);
        if (!sceneProvided && !meshProvided) {
            documentLabel = diagnosticsPath.stem().string();
            if (documentLabel.empty()) documentLabel = diagnosticsPath.string();
            sessionKind = "diagnostics";
        }
    }

    std::cout << "Session panel\n";
    std::cout << "================================================================\n";

    std::cout << "  [document]\n";
    std::cout << "    label: " << documentLabel << "\n";
    std::cout << "    session_kind: " << sessionKind << "\n";
    std::cout << "    dirty: false\n";

    std::cout << "  [scene outline]\n";
    if (!sceneProvided) {
        std::cout << "    scene: (none)\n";
        std::cout << "    entry_count: 0\n";
    } else {
        std::cout << "    scene: " << (sceneName.empty() ? std::string{"(unnamed)"} : sceneName) << "\n";
        std::cout << "    source: " << scenePath.string() << "\n";
        std::cout << "    load_success: " << (sceneOk ? "true" : "false") << "\n";
        std::cout << "    load_valid: " << (sceneValid ? "true" : "false") << "\n";
        std::cout << "    entry_count: " << sceneEntryLines.size() << "\n";
        if (sceneEntryLines.empty()) {
            std::cout << "      (no entries)\n";
        } else {
            for (const auto& line : sceneEntryLines) {
                std::cout << "      - " << line << "\n";
            }
        }
    }

    std::cout << "  [workflow status]\n";
    if (!meshProvided) {
        std::cout << "    mesh: (none)\n";
    } else {
        std::cout << "    mesh: " << meshPath.string() << "\n";
        std::cout << "    import_success: " << (meshOk ? "true" : "false") << "\n";
        std::cout << "    import_valid: " << (meshValid ? "true" : "false") << "\n";
        std::cout << "    vertices: " << meshVertices << "\n";
        std::cout << "    faces: " << meshFaces << "\n";
    }

    std::cout << "  [diagnostics summary]\n";
    if (!diag.present) {
        std::cout << "    source: (none)\n";
    } else {
        std::cout << "    source: " << diag.source.string() << "\n";
        std::cout << "    read_success: " << (diag.readSucceeded ? "true" : "false") << "\n";
        auto emit = [](const char* key, const std::optional<unsigned long>& v) {
            std::cout << "    " << key << ": ";
            if (v.has_value()) std::cout << *v; else std::cout << "(missing)";
            std::cout << "\n";
        };
        emit("non_manifold_edges", diag.nonManifoldEdges);
        emit("boundary_edges",     diag.boundaryEdges);
        emit("degenerate_faces",   diag.degenerateFaces);
        emit("isolated_vertices",  diag.isolatedVertices);
    }

    // Read-only panel: orientation only. Always exits cleanly even when inputs
    // are missing — the empty state is itself a valid panel rendering.
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// PR-18: persistence and recovery basics.
//
// State is stored as plain key:value text under a deterministic directory so
// the app can be restarted into its last workspace state without depending on
// the kernel or any external storage. Test isolation is achieved via the
// --state-dir flag; production defaults to ${HOME}/.local/state/nexus_modeling
// when HOME is set and to /tmp/nexus_modeling_state otherwise.
//
// Recovery is intentionally permissive: missing files, truncated lines, and
// unknown keys never crash. Each parse failure is logged and rendered.
// ─────────────────────────────────────────────────────────────────────────────
std::filesystem::path defaultStateDir()
{
    if (const char* env = std::getenv("NEXUS_MODELING_STATE_DIR"); env && *env) {
        return std::filesystem::path{env};
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path{home} / ".local" / "state" / "nexus_modeling";
    }
    return std::filesystem::path{"/tmp/nexus_modeling_state"};
}

std::filesystem::path stateSessionFilePath(const std::filesystem::path& stateDir)
{
    return stateDir / "last_session.txt";
}

std::filesystem::path stateLogFilePath(const std::filesystem::path& stateDir)
{
    return stateDir / "app.log";
}

bool ensureStateDir(const std::filesystem::path& stateDir, std::string& errorOut)
{
    std::error_code ec;
    std::filesystem::create_directories(stateDir, ec);
    if (ec) {
        errorOut = "Failed to create state dir " + stateDir.string() + ": " + ec.message();
        return false;
    }
    return true;
}

struct SessionStateRecord {
    std::string sessionKind;
    std::string sessionLabel;
    std::string lastMesh;
    std::string lastScene;
    std::string lastDiagnostics;
    std::string savedAtUnix;
    std::vector<std::string> unknownKeys;
    std::vector<std::string> malformedLines;
};

bool readSessionStateRecord(const std::filesystem::path& path,
                            SessionStateRecord& record)
{
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            record.malformedLines.push_back(line);
            continue;
        }
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.erase(value.begin());
        }
        if      (key == "session_kind")     record.sessionKind     = value;
        else if (key == "session_label")    record.sessionLabel    = value;
        else if (key == "last_mesh")        record.lastMesh        = value;
        else if (key == "last_scene")       record.lastScene       = value;
        else if (key == "last_diagnostics") record.lastDiagnostics = value;
        else if (key == "saved_at_unix")    record.savedAtUnix     = value;
        else                                record.unknownKeys.push_back(key);
    }
    return true;
}

int runSaveSessionState(const std::filesystem::path& stateDir,
                        const std::string& sessionKind,
                        const std::string& sessionLabel,
                        const std::filesystem::path& meshPath,
                        const std::filesystem::path& scenePath,
                        const std::filesystem::path& diagnosticsPath)
{
    if (sessionKind.empty()) {
        std::cerr << "Missing --session-kind for --save-session-state\n";
        std::cerr << kUsage;
        return 2;
    }
    std::string err;
    if (!ensureStateDir(stateDir, err)) {
        std::cerr << err << "\n";
        return 1;
    }
    const auto file = stateSessionFilePath(stateDir);
    std::ofstream out(file, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Failed to open state file " << file.string() << " for writing\n";
        return 1;
    }
    const auto nowUnix = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
    out << "session_kind: "     << sessionKind << "\n";
    out << "session_label: "    << sessionLabel << "\n";
    out << "last_mesh: "        << meshPath.string() << "\n";
    out << "last_scene: "       << scenePath.string() << "\n";
    out << "last_diagnostics: " << diagnosticsPath.string() << "\n";
    out << "saved_at_unix: "    << nowUnix << "\n";
    if (!out) {
        std::cerr << "Failed to write state file " << file.string() << "\n";
        return 1;
    }

    std::cout << "Session state saved\n";
    std::cout << "================================================================\n";
    std::cout << "  state_dir: " << stateDir.string() << "\n";
    std::cout << "  state_file: " << file.string() << "\n";
    std::cout << "  session_kind: " << sessionKind << "\n";
    std::cout << "  session_label: " << sessionLabel << "\n";
    std::cout << "  last_mesh: " << meshPath.string() << "\n";
    std::cout << "  last_scene: " << scenePath.string() << "\n";
    std::cout << "  last_diagnostics: " << diagnosticsPath.string() << "\n";
    std::cout << "  saved_at_unix: " << nowUnix << "\n";
    return 0;
}

int runRestoreSessionState(const std::filesystem::path& stateDir)
{
    const auto file = stateSessionFilePath(stateDir);
    const bool exists = std::filesystem::exists(file);

    std::cout << "Restored session state\n";
    std::cout << "================================================================\n";
    std::cout << "  state_dir: " << stateDir.string() << "\n";
    std::cout << "  state_file: " << file.string() << "\n";
    std::cout << "  state_exists: " << (exists ? "true" : "false") << "\n";

    auto emit = [](const char* key, const std::string& v) {
        std::cout << "  " << key << ": " << (v.empty() ? std::string{"(missing)"} : v) << "\n";
    };

    if (!exists) {
        // Empty-state recovery: the panel still renders so the user gets a
        // consistent surface and the app boots without throwing on absent state.
        emit("session_kind", "");
        emit("session_label", "");
        emit("last_mesh", "");
        emit("last_scene", "");
        emit("last_diagnostics", "");
        emit("saved_at_unix", "");
        return 0;
    }

    SessionStateRecord record{};
    const bool readOk = readSessionStateRecord(file, record);
    std::cout << "  read_success: " << (readOk ? "true" : "false") << "\n";
    emit("session_kind",     record.sessionKind);
    emit("session_label",    record.sessionLabel);
    emit("last_mesh",        record.lastMesh);
    emit("last_scene",       record.lastScene);
    emit("last_diagnostics", record.lastDiagnostics);
    emit("saved_at_unix",    record.savedAtUnix);
    std::cout << "  unknown_key_count: "    << record.unknownKeys.size() << "\n";
    for (const auto& k : record.unknownKeys) {
        std::cout << "    - unknown_key: " << k << "\n";
    }
    std::cout << "  malformed_line_count: " << record.malformedLines.size() << "\n";
    return 0;
}

int runAppendStateLog(const std::filesystem::path& stateDir, const std::string& message)
{
    if (message.empty()) {
        std::cerr << "Missing message for --append-state-log\n";
        std::cerr << kUsage;
        return 2;
    }
    std::string err;
    if (!ensureStateDir(stateDir, err)) {
        std::cerr << err << "\n";
        return 1;
    }
    const auto file = stateLogFilePath(stateDir);
    std::ofstream out(file, std::ios::app);
    if (!out.is_open()) {
        std::cerr << "Failed to open log file " << file.string() << " for append\n";
        return 1;
    }
    const auto nowUnix = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
    out << nowUnix << " " << message << "\n";

    std::cout << "State log appended\n";
    std::cout << "================================================================\n";
    std::cout << "  state_dir: " << stateDir.string() << "\n";
    std::cout << "  log_file: " << file.string() << "\n";
    std::cout << "  message: " << message << "\n";
    return 0;
}

int runShowStateLog(const std::filesystem::path& stateDir)
{
    const auto file = stateLogFilePath(stateDir);
    const bool exists = std::filesystem::exists(file);

    std::cout << "State log\n";
    std::cout << "================================================================\n";
    std::cout << "  state_dir: " << stateDir.string() << "\n";
    std::cout << "  log_file: " << file.string() << "\n";
    std::cout << "  log_exists: " << (exists ? "true" : "false") << "\n";
    if (!exists) {
        std::cout << "  line_count: 0\n";
        return 0;
    }
    std::ifstream in(file);
    if (!in.is_open()) {
        std::cerr << "Failed to open log file " << file.string() << " for read\n";
        return 1;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    std::cout << "  line_count: " << lines.size() << "\n";
    for (const auto& l : lines) {
        std::cout << "    - " << l << "\n";
    }
    return 0;
}

int runLoadScene(const std::filesystem::path& path, bool emitSessionSummary)
{
    if (path.empty()) {
        std::cerr << "Missing path for --load-scene\n";
        return 2;
    }
    nexus::asset::SceneAsset scene;
    const auto report = scene.load(path.string());

    std::cout << "Scene asset load\n";
    std::cout << "================================================================\n";
    std::cout << "  path: " << path.string() << "\n";
    std::cout << "  success: " << (report.isSuccess() ? "true" : "false") << "\n";
    std::cout << "  valid: " << (report.valid ? "true" : "false") << "\n";
    std::cout << "  scene_name: " << scene.sceneName() << "\n";
    std::cout << "  entry_count: " << report.entryCount << "\n";
    std::cout << "  version: " << report.version << "\n";
    for (const auto& msg : report.messages) {
        std::cout << "  message: " << msg << "\n";
    }
    if (emitSessionSummary) {
        SessionModel session;
        session.kind = SessionModel::Kind::LoadScene;
        session.label = path.string();
        session.ready = report.isSuccess() && report.valid;
        session.sessionSuccess = report.isSuccess();
        session.workflowStepCount = 0;
        session.diagnosticsValid = false;
        session.artifactPaths.push_back(path);
        printSessionSummary(session);
    }
    return report.isSuccess() ? 0 : 1;
}

int main(int argc, char** argv)
{
    bool listScenarios = false;
    bool showDashboard = false;
    bool showShell = false;
    bool showArtifacts = false;
    bool runScenario = false;
    bool refreshDashboard = false;
    bool skipGates = false;
    bool starterWorkspace = false;
    bool starterModel = false;
    bool starterCleanup = false;
    bool importMesh = false;
    bool loadScene = false;
    bool sessionSummary = false;
    bool editorSeamsDemo = false;
    bool sessionPanel = false;
    bool saveSessionState = false;
    bool restoreSessionState = false;
    bool appendStateLog = false;
    bool showStateLog = false;
    std::string sessionKindArg;
    std::string sessionLabelArg;
    std::string stateLogMessageArg;
    std::filesystem::path stateDirArg;
    std::string starterPrimitiveArg;
    std::filesystem::path importMeshPath;
    std::filesystem::path loadScenePath;
    std::filesystem::path panelMeshPath;
    std::filesystem::path panelScenePath;
    std::filesystem::path panelDiagnosticsPath;
    std::filesystem::path exportMeshPath;
    std::filesystem::path exportDiagnosticsPath;
    float starterSize = 1.0f;
    std::string artifactScenarioId;
    std::string selectedScenarioId;
    std::string runScenarioId;
    std::string runBackendOverride;
    std::filesystem::path manifestPath = defaultManifestPath();
    std::filesystem::path dashboardPath = defaultDashboardPath();
    std::filesystem::path knownGapsPath = defaultKnownGapsPath();
    std::filesystem::path artifactsRoot = defaultArtifactsRoot();
    std::filesystem::path runnerScript = defaultRunScenarioScript();
    std::filesystem::path dashboardScript = defaultDashboardScript();

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i] ? argv[i] : "";
        if (arg == "--help" || arg == "-h") {
            std::cout << kUsage;
            return 0;
        }
        if (arg == "--list-scenarios") {
            listScenarios = true;
            continue;
        }
        if (arg == "--show-dashboard") {
            showDashboard = true;
            continue;
        }
        if (arg == "--shell") {
            showShell = true;
            continue;
        }
        if (arg == "--show-artifacts") {
            showArtifacts = true;
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --show-artifacts\n";
                std::cerr << kUsage;
                return 2;
            }
            artifactScenarioId = argv[++i];
            continue;
        }
        if (arg == "--run-scenario") {
            runScenario = true;
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --run-scenario\n";
                std::cerr << kUsage;
                return 2;
            }
            runScenarioId = argv[++i];
            continue;
        }
        if (arg == "--backend") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --backend\n";
                std::cerr << kUsage;
                return 2;
            }
            runBackendOverride = argv[++i];
            continue;
        }
        if (arg == "--runner") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --runner\n";
                std::cerr << kUsage;
                return 2;
            }
            runnerScript = argv[++i];
            continue;
        }
        if (arg == "--scenario") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --scenario\n";
                std::cerr << kUsage;
                return 2;
            }
            selectedScenarioId = argv[++i];
            continue;
        }
        if (arg == "--artifacts-root") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --artifacts-root\n";
                std::cerr << kUsage;
                return 2;
            }
            artifactsRoot = argv[++i];
            continue;
        }
        if (arg == "--manifest") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --manifest\n";
                std::cerr << kUsage;
                return 2;
            }
            manifestPath = argv[++i];
            continue;
        }
        if (arg == "--dashboard") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --dashboard\n";
                std::cerr << kUsage;
                return 2;
            }
            dashboardPath = argv[++i];
            continue;
        }
        if (arg == "--known-gaps") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --known-gaps\n";
                std::cerr << kUsage;
                return 2;
            }
            knownGapsPath = argv[++i];
            continue;
        }
        if (arg == "--refresh-dashboard") {
            refreshDashboard = true;
            continue;
        }
        if (arg == "--dashboard-script") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --dashboard-script\n";
                std::cerr << kUsage;
                return 2;
            }
            dashboardScript = argv[++i];
            continue;
        }
        if (arg == "--skip-gates") {
            skipGates = true;
            continue;
        }
        if (arg == "--starter-workspace") {
            starterWorkspace = true;
            continue;
        }
        if (arg == "--starter-model") {
            starterModel = true;
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --starter-model\n";
                std::cerr << kUsage;
                return 2;
            }
            starterPrimitiveArg = argv[++i];
            continue;
        }
        if (arg == "--size") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --size\n";
                std::cerr << kUsage;
                return 2;
            }
            try {
                starterSize = std::stof(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid value for --size: " << argv[i] << "\n";
                return 2;
            }
            continue;
        }
        if (arg == "--cleanup") {
            starterCleanup = true;
            continue;
        }
        if (arg == "--export-mesh") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --export-mesh\n";
                std::cerr << kUsage;
                return 2;
            }
            exportMeshPath = argv[++i];
            continue;
        }
        if (arg == "--export-diagnostics") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --export-diagnostics\n";
                std::cerr << kUsage;
                return 2;
            }
            exportDiagnosticsPath = argv[++i];
            continue;
        }
        if (arg == "--import-mesh") {
            importMesh = true;
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --import-mesh\n";
                std::cerr << kUsage;
                return 2;
            }
            importMeshPath = argv[++i];
            continue;
        }
        if (arg == "--load-scene") {
            loadScene = true;
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --load-scene\n";
                std::cerr << kUsage;
                return 2;
            }
            loadScenePath = argv[++i];
            continue;
        }
        if (arg == "--session-summary") {
            sessionSummary = true;
            continue;
        }
        if (arg == "--editor-seams-demo") {
            editorSeamsDemo = true;
            continue;
        }
        if (arg == "--session-panel") {
            sessionPanel = true;
            continue;
        }
        if (arg == "--panel-mesh") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --panel-mesh\n";
                std::cerr << kUsage;
                return 2;
            }
            panelMeshPath = argv[++i];
            continue;
        }
        if (arg == "--panel-scene") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --panel-scene\n";
                std::cerr << kUsage;
                return 2;
            }
            panelScenePath = argv[++i];
            continue;
        }
        if (arg == "--panel-diagnostics") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --panel-diagnostics\n";
                std::cerr << kUsage;
                return 2;
            }
            panelDiagnosticsPath = argv[++i];
            continue;
        }
        if (arg == "--save-session-state") {
            saveSessionState = true;
            continue;
        }
        if (arg == "--restore-session-state") {
            restoreSessionState = true;
            continue;
        }
        if (arg == "--append-state-log") {
            appendStateLog = true;
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --append-state-log\n";
                std::cerr << kUsage;
                return 2;
            }
            stateLogMessageArg = argv[++i];
            continue;
        }
        if (arg == "--show-state-log") {
            showStateLog = true;
            continue;
        }
        if (arg == "--session-kind") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --session-kind\n";
                std::cerr << kUsage;
                return 2;
            }
            sessionKindArg = argv[++i];
            continue;
        }
        if (arg == "--session-label") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --session-label\n";
                std::cerr << kUsage;
                return 2;
            }
            sessionLabelArg = argv[++i];
            continue;
        }
        if (arg == "--state-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --state-dir\n";
                std::cerr << kUsage;
                return 2;
            }
            stateDirArg = argv[++i];
            continue;
        }

        std::cerr << "Unknown argument: " << arg << '\n';
        std::cerr << kUsage;
        return 2;
    }

    if (listScenarios) {
        std::vector<ScenarioEntry> entries;
        std::string error;
        if (!loadScenarioManifest(manifestPath, entries, error)) {
            std::cerr << error << '\n';
            return 1;
        }

        std::cout << "Available scenarios:\n";
        for (const ScenarioEntry& scenario : entries) {
            std::cout << "  - " << scenario.id << " (" << scenario.backend
                      << "): " << scenario.description << '\n';
        }
        return 0;
    }

    if (showDashboard) {
        if (!std::filesystem::exists(dashboardPath)) {
            std::cout << "Capability dashboard not found at " << dashboardPath.string() << "\n";
            std::cout << "Empty-state: run tools/capability_dashboard.sh to generate it.\n";
            return 0;
        }

        std::string json;
        std::string error;
        if (!loadFileText(dashboardPath, json, error)) {
            std::cerr << error << '\n';
            return 1;
        }

        const std::string generatedAt = extractJsonStringValue(json, "generated_at_utc");
        const std::string commit = extractJsonStringValue(json, "git_commit");
        const std::string deterministicStatus =
            extractJsonStringValueAfterAnchor(json, "deterministic_checks", "status");

        std::cout << "Capability dashboard: " << dashboardPath.string() << "\n";
        if (!generatedAt.empty()) {
            std::cout << "  generated_at_utc: " << generatedAt << "\n";
        }
        if (!commit.empty()) {
            std::cout << "  git_commit: " << commit << "\n";
        }
        if (!deterministicStatus.empty()) {
            std::cout << "  deterministic_checks.status: " << deterministicStatus << "\n";
        }

        const auto subsystemStatuses = extractSubsystemStatuses(json);
        if (subsystemStatuses.empty()) {
            std::cout << "  subsystem summary: unavailable (unexpected dashboard format)\n";
        } else {
            std::cout << "  subsystems:\n";
            for (const auto& [name, status] : subsystemStatuses) {
                std::cout << "    - " << name << ": " << status << "\n";
            }
        }

        printKnownGapsReport(loadKnownGaps(knownGapsPath));
        return 0;
    }

    if (showShell) {
        renderShellLayout(manifestPath, dashboardPath, knownGapsPath, artifactsRoot, selectedScenarioId);
        return 0;
    }

    if (starterWorkspace) {
        printStarterWorkspace();
        return 0;
    }

    if (starterModel) {
        return runStarterModel(starterPrimitiveArg, starterSize, starterCleanup,
                               exportMeshPath, exportDiagnosticsPath, sessionSummary);
    }

    if (importMesh) {
        return runImportMesh(importMeshPath, sessionSummary);
    }

    if (loadScene) {
        return runLoadScene(loadScenePath, sessionSummary);
    }

    if (editorSeamsDemo) {
        return runEditorSeamsDemo();
    }

    if (sessionPanel) {
        return runSessionPanel(panelMeshPath, panelScenePath, panelDiagnosticsPath);
    }

    const std::filesystem::path effectiveStateDir =
        stateDirArg.empty() ? defaultStateDir() : stateDirArg;

    if (saveSessionState) {
        return runSaveSessionState(effectiveStateDir, sessionKindArg, sessionLabelArg,
                                   panelMeshPath, panelScenePath, panelDiagnosticsPath);
    }

    if (restoreSessionState) {
        return runRestoreSessionState(effectiveStateDir);
    }

    if (appendStateLog) {
        return runAppendStateLog(effectiveStateDir, stateLogMessageArg);
    }

    if (showStateLog) {
        return runShowStateLog(effectiveStateDir);
    }

    if (showArtifacts) {
        const ScenarioArtifacts a = loadScenarioArtifacts(artifactsRoot, artifactScenarioId);
        printArtifactReport(a);
        if (a.directoryExists && a.summaryFound && a.summaryStatus == "fail") {
            return 1;
        }
        return 0;
    }

    if (runScenario) {
        return runScenarioCommand(runnerScript, runScenarioId, runBackendOverride, artifactsRoot);
    }

    if (refreshDashboard) {
        return refreshDashboardCommand(dashboardScript, dashboardPath, knownGapsPath, skipGates);
    }

    // Week 1 Day 1: startup scaffold only; no editor business logic yet.
    std::cout << "Nexus Tooling App scaffold started.\n";
    return 0;
}
