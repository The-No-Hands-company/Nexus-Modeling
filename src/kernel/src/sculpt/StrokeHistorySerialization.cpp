#include <nexus/sculpt/StrokeHistorySerialization.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

namespace nexus::sculpt {

namespace {

constexpr const char* kMagic = "NEXUS_STROKE_HISTORY_V1";

const char* brushKindToString(BrushKind k) noexcept
{
    switch (k) {
        case BrushKind::Draw:    return "Draw";
        case BrushKind::Smooth:  return "Smooth";
        case BrushKind::Inflate: return "Inflate";
        case BrushKind::Flatten: return "Flatten";
        case BrushKind::Pinch:   return "Pinch";
        case BrushKind::Crease:  return "Crease";
        case BrushKind::Layer:   return "Layer";
        case BrushKind::Grab:    return "Grab";
    }
    return nullptr;
}

bool stringToBrushKind(const std::string& s, BrushKind& out) noexcept
{
    if (s == "Draw")    { out = BrushKind::Draw;    return true; }
    if (s == "Smooth")  { out = BrushKind::Smooth;  return true; }
    if (s == "Inflate") { out = BrushKind::Inflate; return true; }
    if (s == "Flatten") { out = BrushKind::Flatten; return true; }
    if (s == "Pinch")   { out = BrushKind::Pinch;   return true; }
    if (s == "Crease")  { out = BrushKind::Crease;  return true; }
    if (s == "Layer")   { out = BrushKind::Layer;   return true; }
    if (s == "Grab")    { out = BrushKind::Grab;    return true; }
    return false;
}

} // namespace

std::string StrokeHistorySerializer::serialize(
    const std::vector<StrokeDelta>& history,
    StrokeHistorySerializationReport& report)
{
    report = {};

    std::ostringstream out;
    out << kMagic << '\n';

    for (const auto& delta : history) {
        if (!delta.isValid()) {
            report.ok = false;
            report.errors.push_back("invalid StrokeDelta (id == kInvalidStrokeId) skipped");
            continue;
        }
        const char* kindStr = brushKindToString(delta.kind);
        if (!kindStr) {
            report.ok = false;
            report.errors.push_back("unknown BrushKind for stroke id " +
                                    std::to_string(delta.id));
            continue;
        }

        out << "S " << delta.id << ' ' << kindStr << '\n';
        for (const auto& [vi, disp] : delta.vertexDeltas) {
            out << "D " << vi
                << ' ' << std::setprecision(9) << disp.x
                << ' ' << std::setprecision(9) << disp.y
                << ' ' << std::setprecision(9) << disp.z
                << '\n';
        }
        ++report.strokeCount;
    }

    std::sort(report.errors.begin(), report.errors.end());
    if (!report.ok) return {};
    return out.str();
}

StrokeHistorySerializationReport StrokeHistorySerializer::deserialize(
    const std::string& data, std::vector<StrokeDelta>& outHistory)
{
    StrokeHistorySerializationReport report;
    outHistory.clear();

    std::istringstream in(data);
    std::string line;

    if (!std::getline(in, line) || line != kMagic) {
        report.ok = false;
        report.errors.push_back(
            std::string("missing or invalid header (expected ") + kMagic + ")");
        return report;
    }

    StrokeDelta current;
    bool inStroke = false;
    bool strokeError = false; // skip D records for the current erroneous stroke

    auto flushStroke = [&]() {
        if (!inStroke) return;
        if (!strokeError && current.isValid()) {
            outHistory.push_back(std::move(current));
            ++report.strokeCount;
        }
        current = StrokeDelta{};
        inStroke = false;
        strokeError = false;
    };

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag.empty()) continue;

        if (tag == "S") {
            flushStroke();

            StrokeId id = 0;
            std::string kindStr;
            if (!(ss >> id >> kindStr)) {
                report.ok = false;
                report.errors.push_back("malformed S record: " + line);
                inStroke = true;
                strokeError = true;
                continue;
            }
            if (id == kInvalidStrokeId) {
                report.ok = false;
                report.errors.push_back("S record has invalid stroke id 0: " + line);
                inStroke = true;
                strokeError = true;
                continue;
            }
            BrushKind kind{};
            if (!stringToBrushKind(kindStr, kind)) {
                report.ok = false;
                report.errors.push_back("unrecognized BrushKind \"" + kindStr + "\": " + line);
                inStroke = true;
                strokeError = true;
                continue;
            }
            current.id   = id;
            current.kind = kind;
            inStroke = true;
        } else if (tag == "D") {
            if (!inStroke || strokeError) continue;

            uint32_t vi = 0;
            float dx = 0.f, dy = 0.f, dz = 0.f;
            if (!(ss >> vi >> dx >> dy >> dz)) {
                report.ok = false;
                report.errors.push_back("malformed D record: " + line);
                strokeError = true;
                continue;
            }
            current.vertexDeltas.emplace_back(vi, nexus::render::Vec3{dx, dy, dz});
        }
        // Unknown tags are silently skipped (forward-compatible).
    }
    flushStroke();

    // Guarantee the invariant: vertexDeltas sorted ascending by index.
    for (auto& delta : outHistory) {
        std::sort(delta.vertexDeltas.begin(), delta.vertexDeltas.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
    }

    std::sort(report.errors.begin(), report.errors.end());
    return report;
}

} // namespace nexus::sculpt
