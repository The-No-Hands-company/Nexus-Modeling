#include "nexus/scene/NodeScene.h"

namespace nexus {

NodeScene::NodeScene()  = default;
NodeScene::~NodeScene() = default;

// ── Node management ───────────────────────────────────────────────────────────

SceneNodeId NodeScene::addNode(std::string name, NodeKind kind) {
    if (m_nameToId.count(name)) {
        return kInvalidSceneNodeId;
    }
    const SceneNodeId id = m_graph.addNode(kind, name);
    m_nameToId.emplace(name, id);
    m_idToName.emplace(id, std::move(name));
    return id;
}

bool NodeScene::removeNode(SceneNodeId id) noexcept {
    if (!m_graph.removeNode(id)) {
        return false;
    }
    auto it = m_idToName.find(id);
    if (it != m_idToName.end()) {
        m_nameToId.erase(it->second);
        m_idToName.erase(it);
    }
    return true;
}

SceneNodeId NodeScene::nodeByName(const std::string& name) const noexcept {
    auto it = m_nameToId.find(name);
    return it != m_nameToId.end() ? it->second : kInvalidSceneNodeId;
}

bool NodeScene::hasNode(SceneNodeId id) const noexcept {
    return m_graph.hasNode(id);
}

std::string NodeScene::nodeName(SceneNodeId id) const {
    auto it = m_idToName.find(id);
    return it != m_idToName.end() ? it->second : "";
}

NodeKind NodeScene::nodeKind(SceneNodeId id) const noexcept {
    return m_graph.nodeKind(id);
}

std::size_t NodeScene::nodeCount() const noexcept {
    return m_graph.nodeCount();
}

// ── Dependency edges ──────────────────────────────────────────────────────────

bool NodeScene::connect(SceneNodeId srcNode, SceneNodeId dstNode) {
    return m_graph.connect(srcNode, dstNode);
}

bool NodeScene::disconnect(SceneNodeId srcNode, SceneNodeId dstNode) noexcept {
    return m_graph.disconnect(srcNode, dstNode);
}

bool NodeScene::isConnected(SceneNodeId srcNode, SceneNodeId dstNode) const noexcept {
    return m_graph.isConnected(srcNode, dstNode);
}

// ── Asset slots ───────────────────────────────────────────────────────────────

bool NodeScene::setAsset(SceneNodeId id, NodePayload payload) {
    return m_graph.setNodeOutputPayload(id, std::move(payload));
}

bool NodeScene::clearAsset(SceneNodeId id) noexcept {
    return m_graph.clearNodeOutputPayload(id);
}

const NodePayload* NodeScene::asset(SceneNodeId id) const noexcept {
    return m_graph.nodeOutputPayload(id);
}

// ── Cache invalidation ────────────────────────────────────────────────────────

void NodeScene::markDirty(SceneNodeId id) {
    m_graph.markDirty(id);
}

bool NodeScene::isDirty(SceneNodeId id) const noexcept {
    return m_graph.isDirty(id);
}

// ── Evaluation ────────────────────────────────────────────────────────────────

EvalReport NodeScene::evaluate() {
    return m_graph.evaluate();
}

void NodeScene::setComputeCallback(EvalGraph::NodeComputeFn callback) {
    m_graph.setComputeCallback(std::move(callback));
}

void NodeScene::setComputeCallback(EvalGraph::LegacyNodeComputeFn callback) {
    m_graph.setComputeCallback(std::move(callback));
}

// ── Lifetime ──────────────────────────────────────────────────────────────────

void NodeScene::clear() noexcept {
    m_graph.clear();
    m_nameToId.clear();
    m_idToName.clear();
}

// ── Internal graph access ─────────────────────────────────────────────────────

const EvalGraph& NodeScene::evalGraph() const noexcept {
    return m_graph;
}

} // namespace nexus
