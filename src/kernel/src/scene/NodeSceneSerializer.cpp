#include <nexus/scene/NodeSceneSerializer.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <sstream>
#include <string_view>

namespace nexus {

namespace {

// ── Token helpers ─────────────────────────────────────────────────────────────

constexpr std::string_view kMagic = "NEXUS_NODE_SCENE_V1";

bool isValidToken(std::string_view s) noexcept {
    if (s.empty()) return false;
    for (char c : s) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
    }
    return true;
}

std::string_view kindToString(NodeKind k) noexcept {
    switch (k) {
        case NodeKind::Geometry:      return "Geometry";
        case NodeKind::Animation:     return "Animation";
        case NodeKind::Transform:     return "Transform";
        case NodeKind::Merge:         return "Merge";
        case NodeKind::ProxyGeometry: return "ProxyGeometry";
        case NodeKind::Reconstruction:return "Reconstruction";
        case NodeKind::Constant:      return "Constant";
        case NodeKind::Expression:    return "Expression";
    }
    return "Constant"; // unreachable
}

std::optional<NodeKind> stringToKind(std::string_view s) noexcept {
    if (s == "Geometry")       return NodeKind::Geometry;
    if (s == "Animation")      return NodeKind::Animation;
    if (s == "Transform")      return NodeKind::Transform;
    if (s == "Merge")          return NodeKind::Merge;
    if (s == "ProxyGeometry")  return NodeKind::ProxyGeometry;
    if (s == "Reconstruction") return NodeKind::Reconstruction;
    if (s == "Constant")       return NodeKind::Constant;
    if (s == "Expression")     return NodeKind::Expression;
    return std::nullopt;
}

// ── Numeric helpers ───────────────────────────────────────────────────────────

std::string fmtFloat(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    return buf;
}

std::optional<float> parseFloat(std::string_view s) noexcept {
    float v = 0.f;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    return v;
}

std::optional<int64_t> parseInt64(std::string_view s) noexcept {
    int64_t v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    return v;
}

std::optional<uint32_t> parseUint32(std::string_view s) noexcept {
    uint32_t v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    return v;
}

// Split a line on spaces, returning tokens.
std::vector<std::string_view> splitLine(std::string_view line) {
    std::vector<std::string_view> toks;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && line[i] == ' ') ++i;
        if (i >= line.size()) break;
        std::size_t j = i;
        while (j < line.size() && line[j] != ' ') ++j;
        toks.push_back(line.substr(i, j - i));
        i = j;
    }
    return toks;
}

} // namespace

// ── Serialize ─────────────────────────────────────────────────────────────────

std::string NodeSceneSerializer::serialize(
    const NodeScene& scene,
    NodeSceneSerializationReport& report)
{
    report = {};
    std::ostringstream out;
    out << kMagic << '\n';

    const std::vector<SceneNodeId> ids = scene.allNodeIds();

    // Validate all names are valid tokens before writing anything.
    for (SceneNodeId id : ids) {
        const std::string name = scene.nodeName(id);
        if (!isValidToken(name)) {
            report.ok = false;
            report.errors.push_back("node name contains invalid characters: '" + name + "'");
            return {};
        }
    }

    // N records — sorted ascending by id (allNodeIds() already sorted).
    for (SceneNodeId id : ids) {
        out << "N " << id
            << ' ' << kindToString(scene.nodeKind(id))
            << ' ' << scene.nodeName(id) << '\n';
        ++report.nodeCount;
    }

    // E records — data-flow edges, sorted by (src, dst).
    for (SceneNodeId src : ids) {
        for (SceneNodeId dst : scene.outgoingEdges(src)) {
            out << "E " << src << ' ' << dst << '\n';
            ++report.edgeCount;
        }
    }

    // P records — parent-child hierarchy, sorted by child id.
    for (SceneNodeId id : ids) {
        const SceneNodeId par = scene.parent(id);
        if (par != kInvalidSceneNodeId) {
            out << "P " << id << ' ' << par << '\n';
        }
    }

    // A records — asset payloads for supported types.
    for (SceneNodeId id : ids) {
        const NodePayload* p = scene.asset(id);
        if (!p) continue;

        switch (p->type()) {
            case NodePayloadType::None:
                break;
            case NodePayloadType::ScalarF32:
                out << "A " << id << " ScalarF32 " << fmtFloat(*p->scalarF32()) << '\n';
                break;
            case NodePayloadType::IntegerI64:
                out << "A " << id << " IntegerI64 " << *p->integerI64() << '\n';
                break;
            case NodePayloadType::Boolean:
                out << "A " << id << " Boolean " << (*p->boolean() ? '1' : '0') << '\n';
                break;
            case NodePayloadType::ReconstructionDiagnostic: {
                const auto& rd = *p->reconstructionDiagnostic();
                out << "A " << id << " ReconstructionDiagnostic "
                    << fmtFloat(rd.residual) << ' ' << fmtFloat(rd.confidence) << '\n';
                break;
            }
            case NodePayloadType::SimTransform: {
                const auto& st = *p->simTransform();
                out << "A " << id << " SimTransform "
                    << fmtFloat(st.px) << ' ' << fmtFloat(st.py) << ' ' << fmtFloat(st.pz) << ' '
                    << fmtFloat(st.vx) << ' ' << fmtFloat(st.vy) << ' ' << fmtFloat(st.vz) << '\n';
                break;
            }
            // TextUtf8, Binary, SplatCloud: not serialized in this version.
            default:
                break;
        }
    }

    return out.str();
}

// ── Deserialize ───────────────────────────────────────────────────────────────

NodeSceneSerializationReport NodeSceneSerializer::deserialize(
    const std::string& data,
    NodeScene& scene)
{
    NodeSceneSerializationReport report;
    std::istringstream in(data);
    std::string line;

    // Check magic header.
    if (!std::getline(in, line) || line != kMagic) {
        report.ok = false;
        report.errors.push_back("invalid or missing header");
        return report;
    }

    // Two-pass: first collect N/E/P/A records, then apply in order (N first).
    struct NRec { uint32_t id; NodeKind kind; std::string name; };
    struct ERec { uint32_t src, dst; };
    struct PRec { uint32_t child, parent; };
    struct ARec { uint32_t id; NodePayload payload; };

    std::vector<NRec> nRecs;
    std::vector<ERec> eRecs;
    std::vector<PRec> pRecs;
    std::vector<ARec> aRecs;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto toks = splitLine(line);
        if (toks.empty()) continue;

        const std::string_view tag = toks[0];

        if (tag == "N") {
            if (toks.size() < 4) {
                report.ok = false;
                report.errors.push_back("malformed N record: '" + line + "'");
                continue;
            }
            const auto id = parseUint32(toks[1]);
            const auto kind = stringToKind(toks[2]);
            if (!id || !kind) {
                report.ok = false;
                report.errors.push_back("invalid N record fields: '" + line + "'");
                continue;
            }
            nRecs.push_back({*id, *kind, std::string(toks[3])});

        } else if (tag == "E") {
            if (toks.size() < 3) {
                report.ok = false;
                report.errors.push_back("malformed E record: '" + line + "'");
                continue;
            }
            const auto src = parseUint32(toks[1]);
            const auto dst = parseUint32(toks[2]);
            if (!src || !dst) {
                report.ok = false;
                report.errors.push_back("invalid E record fields: '" + line + "'");
                continue;
            }
            eRecs.push_back({*src, *dst});

        } else if (tag == "P") {
            if (toks.size() < 3) {
                report.ok = false;
                report.errors.push_back("malformed P record: '" + line + "'");
                continue;
            }
            const auto child = parseUint32(toks[1]);
            const auto par   = parseUint32(toks[2]);
            if (!child || !par) {
                report.ok = false;
                report.errors.push_back("invalid P record fields: '" + line + "'");
                continue;
            }
            pRecs.push_back({*child, *par});

        } else if (tag == "A") {
            if (toks.size() < 3) {
                report.ok = false;
                report.errors.push_back("malformed A record: '" + line + "'");
                continue;
            }
            const auto id = parseUint32(toks[1]);
            if (!id) {
                report.ok = false;
                report.errors.push_back("invalid A record id: '" + line + "'");
                continue;
            }
            const std::string_view ptype = toks[2];
            NodePayload payload;
            bool ok = true;

            if (ptype == "ScalarF32") {
                if (toks.size() < 4) { ok = false; }
                else {
                    auto v = parseFloat(toks[3]);
                    if (!v) ok = false;
                    else payload.value = *v;
                }
            } else if (ptype == "IntegerI64") {
                if (toks.size() < 4) { ok = false; }
                else {
                    auto v = parseInt64(toks[3]);
                    if (!v) ok = false;
                    else payload.value = *v;
                }
            } else if (ptype == "Boolean") {
                if (toks.size() < 4) { ok = false; }
                else {
                    payload.value = (toks[3] == "1");
                }
            } else if (ptype == "ReconstructionDiagnostic") {
                if (toks.size() < 5) { ok = false; }
                else {
                    auto res  = parseFloat(toks[3]);
                    auto conf = parseFloat(toks[4]);
                    if (!res || !conf) ok = false;
                    else payload.value = NodePayload::ReconstructionDiagnostic{*res, *conf};
                }
            } else if (ptype == "SimTransform") {
                if (toks.size() < 9) { ok = false; }
                else {
                    auto px = parseFloat(toks[3]); auto py = parseFloat(toks[4]); auto pz = parseFloat(toks[5]);
                    auto vx = parseFloat(toks[6]); auto vy = parseFloat(toks[7]); auto vz = parseFloat(toks[8]);
                    if (!px || !py || !pz || !vx || !vy || !vz) ok = false;
                    else payload.value = NodePayload::SimTransform{*px,*py,*pz,*vx,*vy,*vz};
                }
            } else {
                // Unknown payload type — silently skip (forward-compatible).
                continue;
            }

            if (!ok) {
                report.ok = false;
                report.errors.push_back("malformed A record value: '" + line + "'");
                continue;
            }
            aRecs.push_back({*id, std::move(payload)});

        } else {
            // Unknown tag — silently skip (forward-compatible).
        }
    }

    // The serialized ids are the original NodeScene ids, but NodeScene::addNode
    // assigns its own ids internally.  We build an old-id → new-id map so that
    // E and P records can be resolved correctly.
    std::unordered_map<uint32_t, SceneNodeId> idMap;

    // Sort N records by id for deterministic insertion order.
    std::sort(nRecs.begin(), nRecs.end(),
              [](const NRec& a, const NRec& b){ return a.id < b.id; });

    for (const auto& n : nRecs) {
        const SceneNodeId newId = scene.addNode(n.name, n.kind);
        if (newId == kInvalidSceneNodeId) {
            report.ok = false;
            report.errors.push_back("failed to add node '" + n.name + "' (duplicate name?)");
            continue;
        }
        idMap[n.id] = newId;
        ++report.nodeCount;
    }

    for (const auto& e : eRecs) {
        auto srcIt = idMap.find(e.src);
        auto dstIt = idMap.find(e.dst);
        if (srcIt == idMap.end() || dstIt == idMap.end()) {
            report.ok = false;
            report.errors.push_back("E record references unknown node id");
            continue;
        }
        [[maybe_unused]] bool ok = scene.connect(srcIt->second, dstIt->second);
        ++report.edgeCount;
    }

    for (const auto& p : pRecs) {
        auto childIt  = idMap.find(p.child);
        auto parentIt = idMap.find(p.parent);
        if (childIt == idMap.end() || parentIt == idMap.end()) {
            report.ok = false;
            report.errors.push_back("P record references unknown node id");
            continue;
        }
        [[maybe_unused]] bool ok = scene.setParent(childIt->second, parentIt->second);
    }

    for (const auto& a : aRecs) {
        auto it = idMap.find(a.id);
        if (it == idMap.end()) {
            report.ok = false;
            report.errors.push_back("A record references unknown node id");
            continue;
        }
        [[maybe_unused]] bool ok = scene.setAsset(it->second, a.payload);
    }

    return report;
}

} // namespace nexus
