#include "nexus/eval/EvalGraph.h"

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace nexus {

EvalGraph::EvalGraph()  = default;
EvalGraph::~EvalGraph() = default;

// ── Node management ───────────────────────────────────────────────────────────

NodeId EvalGraph::addNode(NodeKind kind, std::string name) {
    NodeId id = m_nextId++;
    m_nodes.emplace(id, Node{id, kind, std::move(name), /*dirty=*/true});
    return id;
}

bool EvalGraph::removeNode(NodeId id) noexcept {
    if (m_nodes.erase(id) == 0) return false;

    m_edges.erase(
        std::remove_if(m_edges.begin(), m_edges.end(),
                       [id](const Edge& e){ return e.src == id || e.dst == id; }),
        m_edges.end());
    return true;
}

bool EvalGraph::hasNode(NodeId id) const noexcept {
    return findNode(id) != nullptr;
}

NodeKind EvalGraph::nodeKind(NodeId id) const noexcept {
    const Node* n = findNode(id);
    return n ? n->kind : NodeKind::Constant;
}

std::string EvalGraph::nodeName(NodeId id) const {
    const Node* n = findNode(id);
    return n ? n->name : "";
}

std::size_t EvalGraph::nodeCount() const noexcept {
    return m_nodes.size();
}

// ── Edge management ───────────────────────────────────────────────────────────

bool EvalGraph::connect(NodeId srcNode, NodeId dstNode) {
    if (srcNode == dstNode)                       return false;
    if (!hasNode(srcNode) || !hasNode(dstNode))   return false;
    if (isConnected(srcNode, dstNode))            return false;
    m_edges.push_back(Edge{srcNode, dstNode});
    return true;
}

bool EvalGraph::disconnect(NodeId srcNode, NodeId dstNode) noexcept {
    auto it = std::find_if(m_edges.begin(), m_edges.end(),
                           [srcNode, dstNode](const Edge& e){
                               return e.src == srcNode && e.dst == dstNode;
                           });
    if (it == m_edges.end()) return false;
    m_edges.erase(it);
    return true;
}

bool EvalGraph::isConnected(NodeId srcNode, NodeId dstNode) const noexcept {
    return std::any_of(m_edges.begin(), m_edges.end(),
                       [srcNode, dstNode](const Edge& e){
                           return e.src == srcNode && e.dst == dstNode;
                       });
}

// ── Cache invalidation ────────────────────────────────────────────────────────

void EvalGraph::markDirty(NodeId id) {
    for (NodeId did : downstreamOf(id)) {
        Node* n = findNode(did);
        if (n) n->dirty = true;
    }
}

bool EvalGraph::isDirty(NodeId id) const noexcept {
    const Node* n = findNode(id);
    return n && n->dirty;
}

void EvalGraph::clearDirtyAll() noexcept {
    for (auto& [key, n] : m_nodes) n.dirty = false;
}

// ── Cycle detection ───────────────────────────────────────────────────────────

bool EvalGraph::hasCycle() const {
    bool cycle = false;
    (void)topoSort(cycle);
    return cycle;
}

// ── Evaluation ────────────────────────────────────────────────────────────────

EvalReport EvalGraph::evaluate() {
    EvalReport report;

    bool cycle = false;
    report.evaluationOrder = topoSort(cycle);
    if (cycle) {
        report.ok       = false;
        report.hasCycle = true;
        return report;
    }

    for (NodeId id : report.evaluationOrder) {
        auto it = m_nodes.find(id);
        if (it == m_nodes.end()) continue;
        Node& n = it->second;
        if (n.dirty) {
            if (m_computeCallback && !m_computeCallback(n.id, n.kind, n.name)) {
                report.ok = false;
                report.executionFailed = true;
                report.failedNode = n.id;
                return report;
            }
            report.dirtyNodes.push_back(id);
            n.dirty = false;
        }
    }
    return report;
}

void EvalGraph::setComputeCallback(NodeComputeFn callback) {
    m_computeCallback = std::move(callback);
}

// ── Lifetime ──────────────────────────────────────────────────────────────────

void EvalGraph::clear() noexcept {
    m_nodes.clear();
    m_edges.clear();
    // m_nextId not reset: ids must not be reused across a clear.
}

// ── Private helpers ───────────────────────────────────────────────────────────

EvalGraph::Node* EvalGraph::findNode(NodeId id) noexcept {
    auto it = m_nodes.find(id);
    return it != m_nodes.end() ? &it->second : nullptr;
}

const EvalGraph::Node* EvalGraph::findNode(NodeId id) const noexcept {
    auto it = m_nodes.find(id);
    return it != m_nodes.end() ? &it->second : nullptr;
}

std::vector<NodeId> EvalGraph::topoSort(bool& hasCycleOut) const {
    hasCycleOut = false;

    // Kahn's algorithm with a sorted ready-queue for determinism.
    std::unordered_map<NodeId, int> inDegree;
    inDegree.reserve(m_nodes.size());
    for (const auto& [key, n] : m_nodes) inDegree[key] = 0;
    for (const Edge& e : m_edges)  inDegree[e.dst]++;

    // Seed the sorted ready-queue with all zero-in-degree nodes.
    std::vector<NodeId> seeds;
    for (const auto& [key, n] : m_nodes)
        if (inDegree[key] == 0) seeds.push_back(key);
    std::sort(seeds.begin(), seeds.end());

    // Use a deque so pop_front is O(1) rather than O(n).
    std::deque<NodeId> queue(seeds.begin(), seeds.end());

    std::vector<NodeId> order;
    order.reserve(m_nodes.size());

    while (!queue.empty()) {
        NodeId cur = queue.front();
        queue.pop_front();
        order.push_back(cur);

        // Collect successors sorted for determinism.
        std::vector<NodeId> successors;
        for (const Edge& e : m_edges)
            if (e.src == cur) successors.push_back(e.dst);
        std::sort(successors.begin(), successors.end());

        for (NodeId succ : successors) {
            if (--inDegree[succ] == 0) {
                auto pos = std::lower_bound(queue.begin(), queue.end(), succ);
                queue.insert(pos, succ);
            }
        }
    }

    if (order.size() != m_nodes.size()) {
        hasCycleOut = true;
        order.clear();
    }
    return order;
}

std::vector<NodeId> EvalGraph::downstreamOf(NodeId id) const {
    // Forward BFS from id, inclusive (breadth-first traversal of forward edges).
    std::vector<NodeId> result;
    std::unordered_set<NodeId> visited;
    std::deque<NodeId> work;
    work.push_back(id);

    while (!work.empty()) {
        NodeId cur = work.front();
        work.pop_front();
        if (!visited.insert(cur).second) continue;
        result.push_back(cur);
        for (const Edge& e : m_edges)
            if (e.src == cur && visited.find(e.dst) == visited.end())
                work.push_back(e.dst);
    }
    return result;
}

} // namespace nexus
