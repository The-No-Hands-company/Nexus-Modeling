// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Eval Extension — reusable Subgraph templates (Slice 2)
//
//  A SubgraphTemplate is a self-contained, reusable bundle of nodes and edges
//  using a local NodeId namespace. It declares named input and output ports
//  that map to local nodes. `instantiateSubgraph()` clones the template into
//  a host EvalGraph with a name prefix, returning a SubgraphInstance that
//  records the host NodeIds of the input proxies and output nodes.
//
//  Determinism contract:
//    • Template nodes are cloned in ascending local-id order.
//    • Input ports are processed in ascending port-name order.
//    • Output ports are processed in ascending port-name order.
//    • Identical templates instantiated independently produce host node
//      sequences in identical kind/topological order.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <nexus/eval/EvalGraph.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexus::eval_ext {

using LocalNodeId = uint32_t;
inline constexpr LocalNodeId kInvalidLocalNodeId = 0u;

struct SubgraphValidation {
    bool ok = true;
    bool hasCycle = false;
    std::vector<std::string> issues;  ///< Sorted lexicographically.
};

/// Reusable, host-agnostic graph fragment.
class SubgraphTemplate {
public:
    SubgraphTemplate();
    ~SubgraphTemplate();

    // ── Node + edge construction (local namespace) ──────────────────────────
    [[nodiscard]] LocalNodeId addNode(NodeKind kind, std::string name);
    [[nodiscard]] bool        connect(LocalNodeId src, LocalNodeId dst);
    [[nodiscard]] bool        setNodeOutputPayload(LocalNodeId id, NodePayload payload);

    [[nodiscard]] bool        hasNode(LocalNodeId id) const noexcept;
    [[nodiscard]] NodeKind    nodeKind(LocalNodeId id) const noexcept;
    [[nodiscard]] std::string nodeName(LocalNodeId id) const;
    [[nodiscard]] std::size_t nodeCount() const noexcept;
    [[nodiscard]] std::size_t edgeCount() const noexcept;

    // ── Port declaration ─────────────────────────────────────────────────────
    /// Declares a named input entry point routed through local node `id`.
    /// Requirements (checked by validate()):
    ///   • portName is non-empty and unique among input ports.
    ///   • `id` exists in the template.
    ///   • `id` has zero incoming edges in the template (it is a source node).
    [[nodiscard]] bool declareInputPort(std::string portName, LocalNodeId id);

    /// Declares a named output exposing local node `id`.
    /// Requirements: portName non-empty + unique among output ports; `id` exists.
    [[nodiscard]] bool declareOutputPort(std::string portName, LocalNodeId id);

    [[nodiscard]] std::vector<std::string> inputPortNames() const;   ///< Sorted ASC.
    [[nodiscard]] std::vector<std::string> outputPortNames() const;  ///< Sorted ASC.
    [[nodiscard]] LocalNodeId inputPortTarget(std::string_view portName) const noexcept;
    [[nodiscard]] LocalNodeId outputPortTarget(std::string_view portName) const noexcept;

    // ── Validation ──────────────────────────────────────────────────────────
    [[nodiscard]] SubgraphValidation validate() const;

private:
    friend std::optional<struct SubgraphInstance>
    instantiateSubgraph(EvalGraph&, const SubgraphTemplate&, std::string);

    struct Node {
        LocalNodeId id;
        NodeKind    kind;
        std::string name;
        NodePayload payload;  ///< Default monostate; preserved at clone time.
    };
    struct Edge { LocalNodeId src; LocalNodeId dst; };

    std::unordered_map<LocalNodeId, Node> m_nodes;
    std::vector<Edge>                     m_edges;
    std::map<std::string, LocalNodeId>    m_inputPorts;   ///< Ordered by key.
    std::map<std::string, LocalNodeId>    m_outputPorts;
    LocalNodeId                           m_nextId = 1u;
};

/// Result of instantiating a SubgraphTemplate into a host EvalGraph.
struct SubgraphInstance {
    std::string                       prefix;
    /// Port name → host proxy NodeId (Constant kind). Connect external sources here.
    std::map<std::string, NodeId>     inputPorts;
    /// Port name → host NodeId of the cloned output node.
    std::map<std::string, NodeId>     outputPorts;
    /// Every host NodeId created for this instance (input proxies + cloned nodes),
    /// in deterministic creation order.
    std::vector<NodeId>               hostNodes;
    /// Local-template-id → host NodeId (excludes input-port locals; those map to proxies).
    std::unordered_map<LocalNodeId, NodeId> localToHost;
};

/// Clone the template into `host` with the given name prefix.
/// Returns nullopt if the template fails validate() or any host operation fails.
/// On failure no host nodes are committed (best-effort rollback).
[[nodiscard]] std::optional<SubgraphInstance>
instantiateSubgraph(EvalGraph& host, const SubgraphTemplate& tmpl, std::string prefix);

/// Look up the host NodeId for a named input proxy. Returns kInvalidNodeId if unknown.
[[nodiscard]] NodeId subgraphInputProxy(const SubgraphInstance& inst,
                                        std::string_view portName) noexcept;

/// Look up the host NodeId for a named output. Returns kInvalidNodeId if unknown.
[[nodiscard]] NodeId subgraphOutput(const SubgraphInstance& inst,
                                    std::string_view portName) noexcept;

} // namespace nexus::eval_ext
