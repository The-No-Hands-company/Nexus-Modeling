// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Eval Extension — SubgraphTemplate implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/eval_ext/Subgraph.h>

#include <algorithm>
#include <deque>
#include <unordered_set>

namespace nexus::eval_ext {

SubgraphTemplate::SubgraphTemplate()  = default;
SubgraphTemplate::~SubgraphTemplate() = default;

LocalNodeId SubgraphTemplate::addNode(NodeKind kind, std::string name)
{
    const LocalNodeId id = m_nextId++;
    m_nodes.emplace(id, Node{id, kind, std::move(name), {}});
    return id;
}

bool SubgraphTemplate::connect(LocalNodeId src, LocalNodeId dst)
{
    if (src == dst || src == kInvalidLocalNodeId || dst == kInvalidLocalNodeId) {
        return false;
    }
    if (!m_nodes.count(src) || !m_nodes.count(dst)) {
        return false;
    }
    for (const auto& e : m_edges) {
        if (e.src == src && e.dst == dst) {
            return false;  // duplicate
        }
    }
    m_edges.push_back({src, dst});
    return true;
}

bool SubgraphTemplate::setNodeOutputPayload(LocalNodeId id, NodePayload payload)
{
    const auto it = m_nodes.find(id);
    if (it == m_nodes.end()) {
        return false;
    }
    it->second.payload = std::move(payload);
    return true;
}

bool SubgraphTemplate::hasNode(LocalNodeId id) const noexcept
{
    return m_nodes.find(id) != m_nodes.end();
}

NodeKind SubgraphTemplate::nodeKind(LocalNodeId id) const noexcept
{
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? NodeKind::Constant : it->second.kind;
}

std::string SubgraphTemplate::nodeName(LocalNodeId id) const
{
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? std::string{} : it->second.name;
}

std::size_t SubgraphTemplate::nodeCount() const noexcept { return m_nodes.size(); }
std::size_t SubgraphTemplate::edgeCount() const noexcept { return m_edges.size(); }

bool SubgraphTemplate::declareInputPort(std::string portName, LocalNodeId id)
{
    if (portName.empty() || !m_nodes.count(id)) {
        return false;
    }
    if (m_inputPorts.count(portName)) {
        return false;
    }
    m_inputPorts.emplace(std::move(portName), id);
    return true;
}

bool SubgraphTemplate::declareOutputPort(std::string portName, LocalNodeId id)
{
    if (portName.empty() || !m_nodes.count(id)) {
        return false;
    }
    if (m_outputPorts.count(portName)) {
        return false;
    }
    m_outputPorts.emplace(std::move(portName), id);
    return true;
}

std::vector<std::string> SubgraphTemplate::inputPortNames() const
{
    std::vector<std::string> names;
    names.reserve(m_inputPorts.size());
    for (const auto& [k, _] : m_inputPorts) {
        names.push_back(k);
    }
    return names;  // std::map iteration is already sorted.
}

std::vector<std::string> SubgraphTemplate::outputPortNames() const
{
    std::vector<std::string> names;
    names.reserve(m_outputPorts.size());
    for (const auto& [k, _] : m_outputPorts) {
        names.push_back(k);
    }
    return names;
}

LocalNodeId SubgraphTemplate::inputPortTarget(std::string_view portName) const noexcept
{
    const auto it = m_inputPorts.find(std::string{portName});
    return it == m_inputPorts.end() ? kInvalidLocalNodeId : it->second;
}

LocalNodeId SubgraphTemplate::outputPortTarget(std::string_view portName) const noexcept
{
    const auto it = m_outputPorts.find(std::string{portName});
    return it == m_outputPorts.end() ? kInvalidLocalNodeId : it->second;
}

SubgraphValidation SubgraphTemplate::validate() const
{
    SubgraphValidation v;

    // 1) Cycle detection via Kahn's algorithm.
    std::unordered_map<LocalNodeId, int> indegree;
    indegree.reserve(m_nodes.size());
    for (const auto& [id, _] : m_nodes) {
        indegree[id] = 0;
    }
    for (const auto& e : m_edges) {
        ++indegree[e.dst];
    }
    std::deque<LocalNodeId> ready;
    for (const auto& [id, deg] : indegree) {
        if (deg == 0) ready.push_back(id);
    }
    std::size_t visited = 0;
    while (!ready.empty()) {
        const LocalNodeId cur = ready.front();
        ready.pop_front();
        ++visited;
        for (const auto& e : m_edges) {
            if (e.src == cur) {
                if (--indegree[e.dst] == 0) {
                    ready.push_back(e.dst);
                }
            }
        }
    }
    if (visited != m_nodes.size()) {
        v.ok = false;
        v.hasCycle = true;
        v.issues.emplace_back("template contains a directed cycle");
    }

    // 2) Each input port must reference a source-only node.
    for (const auto& [name, id] : m_inputPorts) {
        for (const auto& e : m_edges) {
            if (e.dst == id) {
                v.ok = false;
                v.issues.push_back(
                    "input port '" + name + "' references node with incoming edges");
                break;
            }
        }
    }

    // 3) Input and output port targets must reference existing nodes
    //    (already enforced at declare-time, but re-check after potential removals).
    for (const auto& [name, id] : m_inputPorts) {
        if (!m_nodes.count(id)) {
            v.ok = false;
            v.issues.push_back("input port '" + name + "' references unknown local node");
        }
    }
    for (const auto& [name, id] : m_outputPorts) {
        if (!m_nodes.count(id)) {
            v.ok = false;
            v.issues.push_back("output port '" + name + "' references unknown local node");
        }
    }

    std::sort(v.issues.begin(), v.issues.end());
    return v;
}

// ── Instantiation ────────────────────────────────────────────────────────────

std::optional<SubgraphInstance>
instantiateSubgraph(EvalGraph& host, const SubgraphTemplate& tmpl, std::string prefix)
{
    const auto validation = tmpl.validate();
    if (!validation.ok) {
        return std::nullopt;
    }

    SubgraphInstance inst;
    inst.prefix = std::move(prefix);

    // Collect input-port local ids so we can skip them in the clone pass; they
    // become host Constant proxies instead.
    std::unordered_set<LocalNodeId> inputLocals;
    inputLocals.reserve(tmpl.m_inputPorts.size());
    for (const auto& [name, localId] : tmpl.m_inputPorts) {
        inputLocals.insert(localId);
    }

    auto rollback = [&]() {
        for (auto it = inst.hostNodes.rbegin(); it != inst.hostNodes.rend(); ++it) {
            (void)host.removeNode(*it);
        }
    };

    // Pass 1: input proxies, in sorted port-name order (std::map iteration).
    for (const auto& [portName, localId] : tmpl.m_inputPorts) {
        const std::string hostName = inst.prefix + "/in/" + portName;
        const NodeId hid = host.addNode(NodeKind::Constant, hostName);
        if (hid == kInvalidNodeId) {
            rollback();
            return std::nullopt;
        }
        inst.hostNodes.push_back(hid);
        inst.inputPorts.emplace(portName, hid);
        inst.localToHost.emplace(localId, hid);
    }

    // Pass 2: clone non-input-port nodes in ascending local-id order.
    std::vector<LocalNodeId> sortedLocals;
    sortedLocals.reserve(tmpl.m_nodes.size());
    for (const auto& [id, _] : tmpl.m_nodes) {
        if (!inputLocals.count(id)) {
            sortedLocals.push_back(id);
        }
    }
    std::sort(sortedLocals.begin(), sortedLocals.end());
    for (LocalNodeId localId : sortedLocals) {
        const auto& node = tmpl.m_nodes.at(localId);
        const std::string hostName = inst.prefix + "/" + node.name;
        const NodeId hid = host.addNode(node.kind, hostName);
        if (hid == kInvalidNodeId) {
            rollback();
            return std::nullopt;
        }
        inst.hostNodes.push_back(hid);
        inst.localToHost.emplace(localId, hid);
        if (node.payload.type() != NodePayloadType::None) {
            if (!host.setNodeOutputPayload(hid, node.payload)) {
                rollback();
                return std::nullopt;
            }
        }
    }

    // Pass 3: clone edges in deterministic order (template edge order is the
    // user's insertion order — stable across identical builds).
    for (const auto& e : tmpl.m_edges) {
        const auto sIt = inst.localToHost.find(e.src);
        const auto dIt = inst.localToHost.find(e.dst);
        if (sIt == inst.localToHost.end() || dIt == inst.localToHost.end()) {
            rollback();
            return std::nullopt;
        }
        if (!host.connect(sIt->second, dIt->second)) {
            rollback();
            return std::nullopt;
        }
    }

    // Pass 4: output ports → host NodeIds via the local→host map.
    for (const auto& [portName, localId] : tmpl.m_outputPorts) {
        const auto it = inst.localToHost.find(localId);
        if (it == inst.localToHost.end()) {
            rollback();
            return std::nullopt;
        }
        inst.outputPorts.emplace(portName, it->second);
    }

    return inst;
}

NodeId subgraphInputProxy(const SubgraphInstance& inst, std::string_view portName) noexcept
{
    const auto it = inst.inputPorts.find(std::string{portName});
    return it == inst.inputPorts.end() ? kInvalidNodeId : it->second;
}

NodeId subgraphOutput(const SubgraphInstance& inst, std::string_view portName) noexcept
{
    const auto it = inst.outputPorts.find(std::string{portName});
    return it == inst.outputPorts.end() ? kInvalidNodeId : it->second;
}

} // namespace nexus::eval_ext
