#include <nexus/eval/ExpressionNodeSerializer.h>

#include <algorithm>
#include <sstream>
#include <string_view>

namespace nexus::eval {

namespace {

// Escape double-quotes inside source text so the N record is unambiguous.
std::string escapeSource(std::string_view src) {
    std::string out;
    out.reserve(src.size() + 4);
    for (char c : src) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

// Parse a double-quoted token from the remainder of a line, after the opening ".
// Returns the unescaped content and advances pos past the closing ".
// Returns nullopt on unterminated string.
std::optional<std::string> parseQuoted(std::string_view line, std::size_t& pos) {
    std::string result;
    while (pos < line.size()) {
        char c = line[pos++];
        if (c == '\\' && pos < line.size()) {
            result += line[pos++];
        } else if (c == '"') {
            return result;
        } else {
            result += c;
        }
    }
    return std::nullopt; // unterminated
}

// Split text into non-empty lines.
std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            auto line = text.substr(start, i - start);
            // strip trailing \r
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (!line.empty()) lines.emplace_back(line);
            start = i + 1;
        }
    }
    return lines;
}

// Split a string_view on whitespace, up to maxTokens tokens.
std::vector<std::string_view> tokenize(std::string_view line, std::size_t maxTokens = 64) {
    std::vector<std::string_view> tokens;
    std::size_t i = 0;
    while (i < line.size() && tokens.size() < maxTokens) {
        while (i < line.size() && line[i] == ' ') ++i;
        if (i >= line.size()) break;
        std::size_t start = i;
        while (i < line.size() && line[i] != ' ') ++i;
        tokens.push_back(line.substr(start, i - start));
    }
    return tokens;
}

// Build a name→id lookup for all nodes in graph.
std::unordered_map<std::string, NodeId> buildNameIndex(const EvalGraph& graph) {
    std::unordered_map<std::string, NodeId> idx;
    for (NodeId id : graph.allNodeIds()) {
        idx[graph.nodeName(id)] = id;
    }
    return idx;
}

} // namespace

// ── serialize ─────────────────────────────────────────────────────────────────

std::string ExpressionNodeSerializer::serialize(
    const ExpressionNodeAdapter&       adapter,
    const EvalGraph&                   graph,
    ExpressionNodeSerializationReport& report)
{
    report = {};

    const std::string name = graph.nodeName(adapter.nodeId());
    if (name.empty()) {
        report.ok = false;
        report.errors.push_back("serialize: adapter nodeId not found in graph");
        std::sort(report.errors.begin(), report.errors.end());
        return {};
    }

    const std::string& src = adapter.expression().source();

    std::ostringstream ss;
    ss << "NEXUS_EXPR_NODE_V1\n";
    ss << "N " << name << " \"" << escapeSource(src) << "\"\n";

    for (const auto& b : adapter.bindings()) {
        const std::string srcName = graph.nodeName(b.sourceNode);
        if (srcName.empty()) {
            report.ok = false;
            report.errors.push_back(
                "serialize: binding variable '" + b.variable +
                "' sourceNode not found in graph");
            std::sort(report.errors.begin(), report.errors.end());
            return {};
        }
        ss << "B " << b.variable << " " << srcName << "\n";
        ++report.bindingCount;
    }

    std::sort(report.errors.begin(), report.errors.end());
    return ss.str();
}

// ── deserialize ───────────────────────────────────────────────────────────────

std::optional<ExpressionNodeAdapter> ExpressionNodeSerializer::deserialize(
    const std::string&                 text,
    EvalGraph&                         graph,
    ExpressionNodeSerializationReport& report)
{
    report = {};

    auto lines = splitLines(text);
    if (lines.empty() || lines[0] != "NEXUS_EXPR_NODE_V1") {
        report.ok = false;
        report.errors.push_back("deserialize: missing or invalid header");
        std::sort(report.errors.begin(), report.errors.end());
        return std::nullopt;
    }

    std::string adapterName;
    std::string sourceSrc;
    bool hasN = false;

    struct BindingRecord { std::string variable; std::string sourceNodeName; };
    std::vector<BindingRecord> bindingRecords;

    for (std::size_t li = 1; li < lines.size(); ++li) {
        std::string_view line = lines[li];
        if (line.empty()) continue;

        char tag = line[0];

        if (tag == 'N') {
            // N <name> "<source>"
            auto tokens = tokenize(line, 2); // tag + name; rest is quoted
            if (tokens.size() < 2) {
                report.ok = false;
                report.errors.push_back("deserialize: malformed N record");
                std::sort(report.errors.begin(), report.errors.end());
                return std::nullopt;
            }
            adapterName = std::string(tokens[1]);

            // Find the opening quote.
            std::size_t pos = 2; // skip "N "
            while (pos < line.size() && line[pos] != '"') ++pos;
            if (pos >= line.size() || line[pos] != '"') {
                report.ok = false;
                report.errors.push_back("deserialize: N record missing quoted source");
                std::sort(report.errors.begin(), report.errors.end());
                return std::nullopt;
            }
            ++pos; // skip opening "
            auto src = parseQuoted(line, pos);
            if (!src) {
                report.ok = false;
                report.errors.push_back("deserialize: N record has unterminated source string");
                std::sort(report.errors.begin(), report.errors.end());
                return std::nullopt;
            }
            sourceSrc = std::move(*src);
            hasN = true;
        } else if (tag == 'B') {
            // B <variableName> <sourceNodeName>
            auto tokens = tokenize(line);
            if (tokens.size() < 3) {
                report.ok = false;
                report.errors.push_back("deserialize: malformed B record");
                std::sort(report.errors.begin(), report.errors.end());
                return std::nullopt;
            }
            bindingRecords.push_back({std::string(tokens[1]), std::string(tokens[2])});
        }
        // else: unknown tag — silently skip
    }

    if (!hasN) {
        report.ok = false;
        report.errors.push_back("deserialize: no N record found");
        std::sort(report.errors.begin(), report.errors.end());
        return std::nullopt;
    }

    // Resolve binding source nodes by name.
    auto nameIdx = buildNameIndex(graph);
    std::vector<ExpressionBinding> bindings;
    bindings.reserve(bindingRecords.size());
    for (const auto& br : bindingRecords) {
        auto it = nameIdx.find(br.sourceNodeName);
        if (it == nameIdx.end()) {
            report.ok = false;
            report.errors.push_back(
                "deserialize: binding '" + br.variable +
                "' references unknown node '" + br.sourceNodeName + "'");
            std::sort(report.errors.begin(), report.errors.end());
            return std::nullopt;
        }
        bindings.push_back({br.variable, it->second});
    }

    auto adapter = ExpressionNodeAdapter::create(
        graph, adapterName, sourceSrc, std::move(bindings));
    if (!adapter) {
        report.ok = false;
        report.errors.push_back("deserialize: ExpressionNodeAdapter::create failed "
                                 "(bad expression or missing graph node)");
        std::sort(report.errors.begin(), report.errors.end());
        return std::nullopt;
    }

    report.bindingCount = bindingRecords.size();
    std::sort(report.errors.begin(), report.errors.end());
    return adapter;
}

} // namespace nexus::eval
