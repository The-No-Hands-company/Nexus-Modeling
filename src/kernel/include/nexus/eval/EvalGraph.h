#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus {

using NodeId = uint32_t;
inline constexpr NodeId kInvalidNodeId = 0u;

/// Domain a node operates in.
enum class NodeKind : uint8_t {
    Geometry,   ///< Produces or consumes polygon mesh data.
    Animation,  ///< Produces or consumes animation clip or pose data.
    Transform,  ///< Applies a spatial transform to its input geometry.
    Merge,      ///< Combines multiple geometry inputs into a single output.
    Constant,   ///< Emits a constant value; has no upstream dependencies.
};

/// Result returned by EvalGraph::evaluate().
struct EvalReport {
    bool ok         = true;   ///< False when a cycle is detected or evaluation aborts.
    bool hasCycle   = false;  ///< True when the graph contains a directed cycle.
    bool executionFailed = false; ///< True when a dirty node compute callback returns false.
    NodeId failedNode = kInvalidNodeId; ///< Node that failed execution when executionFailed is true.
    std::vector<std::string> messages; ///< Deterministic diagnostics for cycle/compute failures.
    /// Topologically ordered node sequence (dependencies resolved before dependents).
    std::vector<NodeId> evaluationOrder;
    /// Subset of evaluationOrder that was dirty and therefore re-evaluated.
    std::vector<NodeId> dirtyNodes;
};

/// Rich per-node callback context emitted during evaluate().
struct NodeComputeContext {
    NodeId id = kInvalidNodeId;
    NodeKind kind = NodeKind::Constant;
    std::string name;
    std::vector<NodeId> inputNodes;   ///< Upstream dependencies feeding this node.
    std::vector<NodeId> outputNodes;  ///< Downstream dependents consuming this node.
    std::size_t evaluationIndex = 0;  ///< Index within EvalReport::evaluationOrder.
};

/// Node-based evaluation runtime for procedural geometry and animation workflows.
///
/// Nodes are typed compute units connected by directed edges.  An edge from
/// srcNode to dstNode means srcNode's output feeds into dstNode's input.
/// Evaluation traverses the graph in topological (dependency-first) order and
/// skips clean nodes for incremental updates.
///
/// Thread-safety: none — external locking required for concurrent access.
class EvalGraph {
public:
    using NodeComputeFn = std::function<bool(const NodeComputeContext&)>;
    using LegacyNodeComputeFn = std::function<bool(NodeId, NodeKind, const std::string&)>;

    EvalGraph();
    ~EvalGraph();

    // ── Node management ──────────────────────────────────────────────────────

    /// Add a typed, named node. Returns a stable NodeId that is never reused.
    [[nodiscard]] NodeId addNode(NodeKind kind, std::string name);

    /// Remove a node and all edges incident to it. Returns false if id unknown.
    [[nodiscard]] bool removeNode(NodeId id) noexcept;

    [[nodiscard]] bool        hasNode(NodeId id)  const noexcept;
    [[nodiscard]] NodeKind    nodeKind(NodeId id) const noexcept; ///< Returns Constant for unknown id.
    [[nodiscard]] std::string nodeName(NodeId id) const;           ///< Returns "" for unknown id.
    [[nodiscard]] std::size_t nodeCount()         const noexcept;

    // ── Edge management ──────────────────────────────────────────────────────

    /// Connect srcNode → dstNode. Returns false if either node is unknown,
    /// the ids are identical, or the edge already exists.
    /// Note: not noexcept — edge storage may allocate.
    [[nodiscard]] bool connect(NodeId srcNode, NodeId dstNode);

    /// Remove a directed edge. Returns false if absent or either node unknown.
    [[nodiscard]] bool disconnect(NodeId srcNode, NodeId dstNode) noexcept;

    [[nodiscard]] bool isConnected(NodeId srcNode, NodeId dstNode) const noexcept;

    // ── Cache invalidation ───────────────────────────────────────────────────

    /// Mark a node dirty and transitively dirty all downstream dependents.
    /// Note: not noexcept — downstream traversal may allocate.
    void markDirty(NodeId id);

    /// Returns true when the node is marked dirty. Returns false for unknown ids.
    [[nodiscard]] bool isDirty(NodeId id) const noexcept;

    /// Clear the dirty flag on every node in the graph.
    void clearDirtyAll() noexcept;

    // ── Cycle detection ──────────────────────────────────────────────────────

    /// Returns true when the graph contains at least one directed cycle.
    [[nodiscard]] bool hasCycle() const;

    // ── Evaluation ───────────────────────────────────────────────────────────

    /// Evaluate the graph in topological order.  Dirty nodes are re-evaluated
    /// via optional compute callback and their dirty flags are cleared.
    /// If a cycle is detected the report has ok == false and hasCycle == true;
    /// no nodes are evaluated in that case.
    [[nodiscard]] EvalReport evaluate();

    // Register a rich compute callback invoked for each dirty node in evaluation order.
    // When callback is not set, evaluate() keeps no-op compute behavior.
    void setComputeCallback(NodeComputeFn callback);

    // Backward-compatible callback registration. Wrapped into NodeComputeFn.
    void setComputeCallback(LegacyNodeComputeFn callback);

    // ── Lifetime ─────────────────────────────────────────────────────────────

    /// Remove all nodes and edges.  NodeId counter is not reset.
    void clear() noexcept;

private:
    struct Node {
        NodeId      id;
        NodeKind    kind;
        std::string name;
        bool        dirty = true;
    };

    struct Edge { NodeId src; NodeId dst; };

    std::unordered_map<NodeId, Node> m_nodes;
    std::vector<Edge>                m_edges;
    NodeId            m_nextId = 1u; // 0 reserved as kInvalidNodeId
    NodeComputeFn     m_computeCallback;

    [[nodiscard]] Node*       findNode(NodeId id)       noexcept;
    [[nodiscard]] const Node* findNode(NodeId id) const noexcept;

    /// Kahn's algorithm; sets hasCycleOut and returns empty order on cycle.
    [[nodiscard]] std::vector<NodeId> topoSort(bool& hasCycleOut) const;

    [[nodiscard]] std::vector<NodeId> inputNodesFor(NodeId id) const;
    [[nodiscard]] std::vector<NodeId> outputNodesFor(NodeId id) const;

    /// BFS from id following forward edges; result includes id itself.
    [[nodiscard]] std::vector<NodeId> downstreamOf(NodeId id) const;
};

} // namespace nexus
