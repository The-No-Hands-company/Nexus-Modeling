// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Eval Extension — reusable Subgraph templates (Slice 2-4)
//
//  A SubgraphTemplate is a self-contained, reusable bundle of nodes and edges
//  using a local NodeId namespace. It declares named input and output ports
//  that map to local nodes. `instantiateSubgraph()` clones the template into
//  a host EvalGraph with a name prefix, returning a SubgraphInstance that
//  records the host NodeIds of the input proxies and output nodes.
//
//  Port type contracts (Slice 4):
//    Input/output ports may declare an optional NodePayloadType contract that
//    documents what payload kind the port expects (input) or produces (output).
//    NodePayloadType::None means unconstrained (the default).
//    validate() reports a violation when:
//      • A constrained input port's proxy node carries a payload whose type
//        differs from the declared contract.
//      • A constrained output port's target node carries a payload whose type
//        differs from the declared contract (only when a payload is set).
//    instantiateSubgraph() enforces contracts at instantiation time and returns
//    nullopt when any violation is detected.
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

    /// Declares a named input port with an optional payload type contract.
    /// `contract` = NodePayloadType::None means unconstrained (identical to the
    /// no-contract overload). Any other value is enforced by validate() and
    /// instantiateSubgraph().
    [[nodiscard]] bool declareInputPort(std::string portName, LocalNodeId id,
                                        NodePayloadType contract);

    /// Declares a named output exposing local node `id`.
    /// Requirements: portName non-empty + unique among output ports; `id` exists.
    [[nodiscard]] bool declareOutputPort(std::string portName, LocalNodeId id);

    /// Declares a named output port with an optional payload type contract.
    [[nodiscard]] bool declareOutputPort(std::string portName, LocalNodeId id,
                                          NodePayloadType contract);

    [[nodiscard]] std::vector<std::string> inputPortNames() const;   ///< Sorted ASC.
    [[nodiscard]] std::vector<std::string> outputPortNames() const;  ///< Sorted ASC.
    [[nodiscard]] LocalNodeId inputPortTarget(std::string_view portName) const noexcept;
    [[nodiscard]] LocalNodeId outputPortTarget(std::string_view portName) const noexcept;

    // ── Port type contracts (Slice 4) ────────────────────────────────────────
    /// Return the declared payload-type contract for the named input port.
    /// Returns NodePayloadType::None when the port is unconstrained or unknown.
    [[nodiscard]] NodePayloadType inputPortContract(std::string_view portName) const noexcept;

    /// Return the declared payload-type contract for the named output port.
    /// Returns NodePayloadType::None when the port is unconstrained or unknown.
    [[nodiscard]] NodePayloadType outputPortContract(std::string_view portName) const noexcept;

    /// Update the payload-type contract for an already-declared input port.
    /// Returns false when the port is unknown.
    bool setInputPortContract(std::string_view portName, NodePayloadType contract) noexcept;

    /// Update the payload-type contract for an already-declared output port.
    /// Returns false when the port is unknown.
    bool setOutputPortContract(std::string_view portName, NodePayloadType contract) noexcept;

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

    struct PortEntry {
        LocalNodeId     nodeId   = kInvalidLocalNodeId;
        NodePayloadType contract = NodePayloadType::None;  ///< None = unconstrained.
    };

    std::unordered_map<LocalNodeId, Node> m_nodes;
    std::vector<Edge>                     m_edges;
    std::map<std::string, PortEntry>      m_inputPorts;   ///< Ordered by key.
    std::map<std::string, PortEntry>      m_outputPorts;
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
