#include <nexus/eval_ext/SubgraphSerialization.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

namespace nexus::eval_ext {

namespace {

constexpr const char* kMagic = "NEXUS_SUBGRAPH_V1";

// ── Token validation ──────────────────────────────────────────────────────────

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

// ── NodeKind ↔ string ─────────────────────────────────────────────────────────

const char* kindToString(NodeKind kind) noexcept
{
    switch (kind) {
        case NodeKind::Geometry:       return "Geometry";
        case NodeKind::Animation:      return "Animation";
        case NodeKind::Transform:      return "Transform";
        case NodeKind::Merge:          return "Merge";
        case NodeKind::ProxyGeometry:  return "ProxyGeometry";
        case NodeKind::Reconstruction: return "Reconstruction";
        case NodeKind::Constant:       return "Constant";
        case NodeKind::Expression:     return "Expression";
    }
    return nullptr;
}

bool stringToKind(const std::string& s, NodeKind& out) noexcept
{
    if (s == "Geometry")       { out = NodeKind::Geometry;       return true; }
    if (s == "Animation")      { out = NodeKind::Animation;      return true; }
    if (s == "Transform")      { out = NodeKind::Transform;      return true; }
    if (s == "Merge")          { out = NodeKind::Merge;          return true; }
    if (s == "ProxyGeometry")  { out = NodeKind::ProxyGeometry;  return true; }
    if (s == "Reconstruction") { out = NodeKind::Reconstruction; return true; }
    if (s == "Constant")       { out = NodeKind::Constant;       return true; }
    if (s == "Expression")     { out = NodeKind::Expression;     return true; }
    return false;
}

// ── NodePayloadType ↔ string ──────────────────────────────────────────────────

const char* payloadTypeToString(NodePayloadType t) noexcept
{
    switch (t) {
        case NodePayloadType::None:                  return "None";
        case NodePayloadType::ScalarF32:             return "ScalarF32";
        case NodePayloadType::IntegerI64:            return "IntegerI64";
        case NodePayloadType::Boolean:               return "Boolean";
        case NodePayloadType::TextUtf8:              return "TextUtf8";
        case NodePayloadType::Binary:                return "Binary";
        case NodePayloadType::SplatCloud:            return "SplatCloud";
        case NodePayloadType::ReconstructionDiagnostic: return "ReconstructionDiagnostic";
        case NodePayloadType::SimTransform:          return "SimTransform";
    }
    return "None";
}

bool stringToPayloadType(const std::string& s, NodePayloadType& out) noexcept
{
    if (s == "None")                   { out = NodePayloadType::None;                   return true; }
    if (s == "ScalarF32")              { out = NodePayloadType::ScalarF32;              return true; }
    if (s == "IntegerI64")             { out = NodePayloadType::IntegerI64;             return true; }
    if (s == "Boolean")                { out = NodePayloadType::Boolean;                return true; }
    if (s == "TextUtf8")               { out = NodePayloadType::TextUtf8;               return true; }
    if (s == "Binary")                 { out = NodePayloadType::Binary;                 return true; }
    if (s == "SplatCloud")             { out = NodePayloadType::SplatCloud;             return true; }
    if (s == "ReconstructionDiagnostic") { out = NodePayloadType::ReconstructionDiagnostic; return true; }
    if (s == "SimTransform")           { out = NodePayloadType::SimTransform;           return true; }
    return false;
}

// ── Payload ↔ stream ──────────────────────────────────────────────────────────

// Returns true when the payload was emitted (false for None or unsupported types).
bool serializePayload(const NodePayload& p, std::ostream& out)
{
    switch (p.type()) {
        case NodePayloadType::None:
            return false;
        case NodePayloadType::ScalarF32:
            out << "ScalarF32 " << std::setprecision(9) << *p.scalarF32();
            return true;
        case NodePayloadType::IntegerI64:
            out << "IntegerI64 " << *p.integerI64();
            return true;
        case NodePayloadType::Boolean:
            out << "Boolean " << (*p.boolean() ? '1' : '0');
            return true;
        case NodePayloadType::TextUtf8: {
            const std::string& text = *p.textUtf8();
            if (!isValidToken(text)) return false; // can't encode safely
            out << "TextUtf8 " << text;
            return true;
        }
        default:
            return false; // Binary, SplatCloud, ReconstructionDiagnostic: skip
    }
}

// Parses "type [value]" from the remainder of `ss`. Returns false on format
// error; unknown types are skipped gracefully (payload stays monostate, ok).
bool parsePayload(std::istringstream& ss, NodePayload& out, bool& parseError)
{
    parseError = false;
    std::string type;
    if (!(ss >> type)) { parseError = true; return false; }

    if (type == "ScalarF32") {
        float v = 0.f;
        if (!(ss >> v)) { parseError = true; return false; }
        out.value = v;
        return true;
    }
    if (type == "IntegerI64") {
        int64_t v = 0;
        if (!(ss >> v)) { parseError = true; return false; }
        out.value = v;
        return true;
    }
    if (type == "Boolean") {
        int v = 0;
        if (!(ss >> v)) { parseError = true; return false; }
        out.value = (v != 0);
        return true;
    }
    if (type == "TextUtf8") {
        std::string v;
        if (!(ss >> v) || !isValidToken(v)) { parseError = true; return false; }
        out.value = v;
        return true;
    }
    // Unknown type — skip gracefully (forward-compatibility).
    return false; // payload stays monostate, not an error
}

// Build a dense ascending list of local node ids present in the template.
std::vector<LocalNodeId> collectNodeIds(const SubgraphTemplate& tmpl)
{
    std::vector<LocalNodeId> ids;
    ids.reserve(tmpl.nodeCount());
    for (LocalNodeId id = 1; ids.size() < tmpl.nodeCount(); ++id) {
        if (id > tmpl.nodeCount() * 4 + 32) break; // safety against very sparse ids
        if (tmpl.hasNode(id)) ids.push_back(id);
    }
    return ids;
}

} // namespace

// ── Serialize ─────────────────────────────────────────────────────────────────

std::string SubgraphSerializer::serialize(const SubgraphTemplate& tmpl,
                                          SubgraphSerializationReport& report)
{
    report = {};

    const auto presentIds = collectNodeIds(tmpl);

    // Validate all node names.
    for (LocalNodeId id : presentIds) {
        const std::string name = tmpl.nodeName(id);
        if (!isValidToken(name)) {
            report.ok = false;
            report.errors.push_back("node name contains invalid characters: \"" + name + "\"");
        }
    }
    // Validate port names.
    for (const auto& p : tmpl.inputPortNames()) {
        if (!isValidToken(p)) {
            report.ok = false;
            report.errors.push_back("input port name contains invalid characters: \"" + p + "\"");
        }
    }
    for (const auto& p : tmpl.outputPortNames()) {
        if (!isValidToken(p)) {
            report.ok = false;
            report.errors.push_back("output port name contains invalid characters: \"" + p + "\"");
        }
    }
    std::sort(report.errors.begin(), report.errors.end());
    if (!report.ok) return {};

    // Instantiate into a temp graph to resolve edges and payloads via the
    // public localToHost map on SubgraphInstance.
    EvalGraph tempGraph;
    const auto instOpt = instantiateSubgraph(tempGraph, tmpl, "_ser");
    if (!instOpt) {
        report.ok = false;
        report.errors.push_back("instantiateSubgraph failed");
        return {};
    }
    const auto& inst = *instOpt;

    std::ostringstream out;
    out << kMagic << '\n';

    // N records in ascending local-id order.
    for (LocalNodeId id : presentIds) {
        const char* kindStr = kindToString(tmpl.nodeKind(id));
        if (!kindStr) {
            report.ok = false;
            report.errors.push_back("unknown NodeKind for node id " + std::to_string(id));
            return {};
        }
        out << "N " << id << ' ' << kindStr << ' ' << tmpl.nodeName(id) << '\n';
    }

    // E records: for each ordered local-id pair check temp-graph connectivity.
    for (LocalNodeId src : presentIds) {
        for (LocalNodeId dst : presentIds) {
            if (src == dst) continue;
            auto itSrc = inst.localToHost.find(src);
            auto itDst = inst.localToHost.find(dst);
            if (itSrc == inst.localToHost.end() || itDst == inst.localToHost.end()) continue;
            if (tempGraph.isConnected(itSrc->second, itDst->second)) {
                out << "E " << src << ' ' << dst << '\n';
            }
        }
    }

    // P records: payloads stored on local nodes.
    for (LocalNodeId id : presentIds) {
        auto it = inst.localToHost.find(id);
        if (it == inst.localToHost.end()) continue;
        const NodePayload* p = tempGraph.nodeOutputPayload(it->second);
        if (!p) continue;
        std::ostringstream payloadOut;
        if (serializePayload(*p, payloadOut)) {
            out << "P " << id << ' ' << payloadOut.str() << '\n';
        }
    }

    // I records: input ports sorted ASC (inputPortNames() guarantees this).
    for (const auto& portName : tmpl.inputPortNames()) {
        out << "I " << portName << ' ' << tmpl.inputPortTarget(portName) << '\n';
    }

    // O records: output ports sorted ASC.
    for (const auto& portName : tmpl.outputPortNames()) {
        out << "O " << portName << ' ' << tmpl.outputPortTarget(portName) << '\n';
    }

    // IC / OC records: port type contracts (omitted when None / unconstrained).
    for (const auto& portName : tmpl.inputPortNames()) {
        const NodePayloadType c = tmpl.inputPortContract(portName);
        if (c != NodePayloadType::None) {
            out << "IC " << portName << ' ' << payloadTypeToString(c) << '\n';
        }
    }
    for (const auto& portName : tmpl.outputPortNames()) {
        const NodePayloadType c = tmpl.outputPortContract(portName);
        if (c != NodePayloadType::None) {
            out << "OC " << portName << ' ' << payloadTypeToString(c) << '\n';
        }
    }

    return out.str();
}

// ── Deserialize ───────────────────────────────────────────────────────────────

SubgraphSerializationReport SubgraphSerializer::deserialize(const std::string& data,
                                                             SubgraphTemplate& outTemplate)
{
    SubgraphSerializationReport report;
    outTemplate = SubgraphTemplate{};

    std::istringstream in(data);
    std::string line;

    if (!std::getline(in, line) || line != kMagic) {
        report.ok = false;
        report.errors.push_back("missing or invalid header (expected " +
                                 std::string(kMagic) + ")");
        return report;
    }

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string tagStr;
        ss >> tagStr;
        if (tagStr.empty()) continue;
        const char tag = tagStr[0];

        if (tag == 'N') {
            LocalNodeId id = 0;
            std::string kindStr, name;
            if (!(ss >> id >> kindStr >> name)) {
                report.ok = false;
                report.errors.push_back("malformed N record: " + line);
                continue;
            }
            NodeKind kind;
            if (!stringToKind(kindStr, kind)) {
                report.ok = false;
                report.errors.push_back("unknown NodeKind \"" + kindStr + "\"");
                continue;
            }
            if (!isValidToken(name)) {
                report.ok = false;
                report.errors.push_back("invalid node name token: \"" + name + "\"");
                continue;
            }
            LocalNodeId assigned = outTemplate.addNode(kind, name);
            if (assigned != id) {
                report.ok = false;
                report.errors.push_back("node id mismatch: serialized=" +
                    std::to_string(id) + " assigned=" + std::to_string(assigned));
            }
        } else if (tag == 'E') {
            LocalNodeId src = 0, dst = 0;
            if (!(ss >> src >> dst)) {
                report.ok = false;
                report.errors.push_back("malformed E record: " + line);
                continue;
            }
            if (!outTemplate.connect(src, dst)) {
                report.ok = false;
                report.errors.push_back("connect failed for edge " +
                    std::to_string(src) + "->" + std::to_string(dst));
            }
        } else if (tag == 'P') {
            LocalNodeId id = 0;
            if (!(ss >> id)) {
                report.ok = false;
                report.errors.push_back("malformed P record: " + line);
                continue;
            }
            NodePayload p;
            bool parseError = false;
            parsePayload(ss, p, parseError);
            if (parseError) {
                report.ok = false;
                report.errors.push_back("malformed payload value in P record: " + line);
                continue;
            }
            if (p.type() != NodePayloadType::None) {
                if (!outTemplate.setNodeOutputPayload(id, std::move(p))) {
                    report.ok = false;
                    report.errors.push_back("setNodeOutputPayload failed for node " +
                        std::to_string(id));
                }
            }
        } else if (tagStr == "I") {
            std::string portName;
            LocalNodeId id = 0;
            if (!(ss >> portName >> id)) {
                report.ok = false;
                report.errors.push_back("malformed I record: " + line);
                continue;
            }
            if (!outTemplate.declareInputPort(portName, id)) {
                report.ok = false;
                report.errors.push_back("declareInputPort failed for port \"" + portName + "\"");
            }
        } else if (tagStr == "O") {
            std::string portName;
            LocalNodeId id = 0;
            if (!(ss >> portName >> id)) {
                report.ok = false;
                report.errors.push_back("malformed O record: " + line);
                continue;
            }
            if (!outTemplate.declareOutputPort(portName, id)) {
                report.ok = false;
                report.errors.push_back("declareOutputPort failed for port \"" + portName + "\"");
            }
        } else if (tagStr == "IC") {
            std::string portName, typeStr;
            if (!(ss >> portName >> typeStr)) {
                report.ok = false;
                report.errors.push_back("malformed IC record: " + line);
                continue;
            }
            NodePayloadType contract = NodePayloadType::None;
            if (!stringToPayloadType(typeStr, contract)) {
                continue; // Unknown type — forward-compatible skip.
            }
            if (outTemplate.inputPortTarget(portName) == kInvalidLocalNodeId) {
                report.ok = false;
                report.errors.push_back("IC record references undeclared input port \"" + portName + "\"");
                continue;
            }
            if (!outTemplate.setInputPortContract(portName, contract)) {
                report.ok = false;
                report.errors.push_back("setInputPortContract failed for port \"" + portName + "\"");
            }
        } else if (tagStr == "OC") {
            std::string portName, typeStr;
            if (!(ss >> portName >> typeStr)) {
                report.ok = false;
                report.errors.push_back("malformed OC record: " + line);
                continue;
            }
            NodePayloadType contract = NodePayloadType::None;
            if (!stringToPayloadType(typeStr, contract)) {
                continue; // Unknown type — forward-compatible skip.
            }
            if (outTemplate.outputPortTarget(portName) == kInvalidLocalNodeId) {
                report.ok = false;
                report.errors.push_back("OC record references undeclared output port \"" + portName + "\"");
                continue;
            }
            if (!outTemplate.setOutputPortContract(portName, contract)) {
                report.ok = false;
                report.errors.push_back("setOutputPortContract failed for port \"" + portName + "\"");
            }
        }
        // Unknown tags are silently skipped (forward-compatibility).
    }

    std::sort(report.errors.begin(), report.errors.end());
    return report;
}

} // namespace nexus::eval_ext
