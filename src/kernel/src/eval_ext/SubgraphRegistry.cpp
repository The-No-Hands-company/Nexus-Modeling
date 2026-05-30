#include <nexus/eval_ext/SubgraphRegistry.h>

#include <algorithm>
#include <sstream>

namespace nexus::eval_ext {

namespace {

bool isValidToken(std::string_view s) noexcept
{
    if (s.empty()) return false;
    for (char c : s) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) {
            return false;
        }
    }
    return true;
}

constexpr const char* kRegistryMagic = "NEXUS_SUBGRAPH_REGISTRY_V1";

} // namespace

// ── SubgraphRegistry ──────────────────────────────────────────────────────────

bool SubgraphRegistry::add(std::string name, SubgraphTemplate tmpl)
{
    if (!isValidToken(name)) return false;
    if (m_templates.count(name)) return false;
    m_templates.emplace(std::move(name), std::move(tmpl));
    return true;
}

bool SubgraphRegistry::remove(std::string_view name)
{
    const auto it = m_templates.find(std::string{name});
    if (it == m_templates.end()) return false;
    m_templates.erase(it);
    return true;
}

void SubgraphRegistry::clear() noexcept
{
    m_templates.clear();
}

const SubgraphTemplate* SubgraphRegistry::find(std::string_view name) const noexcept
{
    const auto it = m_templates.find(std::string{name});
    return it == m_templates.end() ? nullptr : &it->second;
}

SubgraphTemplate* SubgraphRegistry::find(std::string_view name) noexcept
{
    const auto it = m_templates.find(std::string{name});
    return it == m_templates.end() ? nullptr : &it->second;
}

bool SubgraphRegistry::contains(std::string_view name) const noexcept
{
    return m_templates.count(std::string{name}) > 0;
}

std::size_t SubgraphRegistry::size() const noexcept
{
    return m_templates.size();
}

bool SubgraphRegistry::empty() const noexcept
{
    return m_templates.empty();
}

std::vector<std::string> SubgraphRegistry::names() const
{
    std::vector<std::string> result;
    result.reserve(m_templates.size());
    for (const auto& [k, _] : m_templates) {
        result.push_back(k);
    }
    return result; // std::map iteration is already sorted
}

// ── SubgraphRegistrySerializer ────────────────────────────────────────────────

std::string SubgraphRegistrySerializer::serialize(
    const SubgraphRegistry& registry,
    SubgraphRegistrySerializationReport& report)
{
    report = {};

    std::ostringstream out;
    out << kRegistryMagic << '\n';

    for (const auto& name : registry.names()) {
        const SubgraphTemplate* tmpl = registry.find(name);
        if (!tmpl) continue; // should never happen

        SubgraphSerializationReport singleReport;
        const std::string body = SubgraphSerializer::serialize(*tmpl, singleReport);
        if (!singleReport.ok) {
            report.ok = false;
            for (const auto& e : singleReport.errors) {
                report.errors.push_back("template \"" + name + "\": " + e);
            }
            continue;
        }

        // Strip the single-template magic header from the body; we embed the
        // remaining records under our T record.
        out << "T " << name << '\n';
        std::istringstream bodyStream(body);
        std::string line;
        bool firstLine = true;
        while (std::getline(bodyStream, line)) {
            if (firstLine) { firstLine = false; continue; } // skip NEXUS_SUBGRAPH_V1
            out << line << '\n';
        }
        ++report.templateCount;
    }

    std::sort(report.errors.begin(), report.errors.end());
    if (!report.ok) return {};
    return out.str();
}

SubgraphRegistrySerializationReport SubgraphRegistrySerializer::deserialize(
    const std::string& data, SubgraphRegistry& outRegistry)
{
    SubgraphRegistrySerializationReport report;
    outRegistry.clear();

    std::istringstream in(data);
    std::string line;

    if (!std::getline(in, line) || line != kRegistryMagic) {
        report.ok = false;
        report.errors.push_back(
            std::string("missing or invalid header (expected ") + kRegistryMagic + ")");
        return report;
    }

    // Collect lines belonging to the current template section.
    std::string currentName;
    std::vector<std::string> currentLines;

    auto flushTemplate = [&]() {
        if (currentName.empty()) return;

        // Reconstruct the single-template text by prepending the magic header.
        std::ostringstream tmplData;
        tmplData << "NEXUS_SUBGRAPH_V1\n";
        for (const auto& l : currentLines) {
            tmplData << l << '\n';
        }

        SubgraphTemplate tmpl;
        const auto sr = SubgraphSerializer::deserialize(tmplData.str(), tmpl);
        if (!sr.ok) {
            report.ok = false;
            for (const auto& e : sr.errors) {
                report.errors.push_back("template \"" + currentName + "\": " + e);
            }
        } else {
            if (!outRegistry.add(currentName, std::move(tmpl))) {
                report.ok = false;
                report.errors.push_back(
                    "duplicate or invalid template name: \"" + currentName + "\"");
            } else {
                ++report.templateCount;
            }
        }

        currentName.clear();
        currentLines.clear();
    };

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "T") {
            flushTemplate(); // commit previous section
            std::string name;
            if (!(ss >> name) || !isValidToken(name)) {
                report.ok = false;
                report.errors.push_back("malformed or invalid T record: " + line);
                currentName.clear();
                continue;
            }
            currentName = std::move(name);
        } else if (!currentName.empty()) {
            currentLines.push_back(line);
        }
        // Lines before the first T record (and unknown top-level tags) are skipped.
    }
    flushTemplate(); // commit last section

    std::sort(report.errors.begin(), report.errors.end());
    return report;
}

} // namespace nexus::eval_ext
