#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Eval — ExpressionNode adapter (Slice 4)
//
//  Bridges `nexus::script::Expression` into the EvalGraph runtime so that a
//  scalar expression's free variables resolve from upstream ScalarF32 payload
//  slots rather than from a hard-coded variable map.
//
//  Typical usage:
//
//    EvalGraph g;
//    NodeId x = g.addNode(NodeKind::Constant, "x");
//    g.setNodeOutputPayload(x, NodePayload{1.5f});
//
//    auto adapter = ExpressionNodeAdapter::create(
//        g, "result", "x * 2 + 1", {{"x", x}});
//    assert(adapter.has_value());
//
//    g.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
//        if (ctx.id == adapter->nodeId()) return adapter->compute(ctx);
//        return true; // passthrough for other nodes
//    });
//    g.evaluate();
//    // g.nodeOutputPayload(adapter->nodeId())->scalarF32() == 4.0
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/eval/EvalGraph.h>
#include <nexus/script/Expression.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::eval {

/// Maps one free variable in an expression to the upstream node whose
/// ScalarF32 output payload carries its value at evaluation time.
struct ExpressionBinding {
    std::string variable;   ///< Free identifier as it appears in the expression source.
    NodeId      sourceNode; ///< Upstream node whose ScalarF32 payload supplies the value.
};

/// Wraps a compiled `nexus::script::Expression` as a node in an `EvalGraph`.
///
/// The adapter owns a `NodeKind::Expression` node and maintains the directed
/// edges from each bound upstream node into it.  At compute time each free
/// variable is resolved from its bound upstream node's ScalarF32 output
/// payload; the evaluated result is written to the adapter node's ScalarF32
/// output payload.
///
/// Thread-safety: none — same contract as EvalGraph.
class ExpressionNodeAdapter {
public:
    /// Compile `source` and add a `NodeKind::Expression` node named `name` to
    /// `graph`.  For every entry in `bindings` the adapter calls
    /// `graph.connect(sourceNode, newNode)`.
    ///
    /// Returns empty when:
    ///   - `source` fails to compile.
    ///   - Any `bindings[i].sourceNode` is not present in `graph`.
    ///   - `bindings[i].variable` is empty.
    [[nodiscard]] static std::optional<ExpressionNodeAdapter> create(
        EvalGraph&                    graph,
        std::string                   name,
        std::string_view              source,
        std::vector<ExpressionBinding> bindings);

    ExpressionNodeAdapter(ExpressionNodeAdapter&&) noexcept;
    ExpressionNodeAdapter& operator=(ExpressionNodeAdapter&&) noexcept;
    ExpressionNodeAdapter(const ExpressionNodeAdapter&) = delete;
    ExpressionNodeAdapter& operator=(const ExpressionNodeAdapter&) = delete;
    ~ExpressionNodeAdapter();

    /// The NodeId allocated in the graph for this expression node.
    [[nodiscard]] NodeId nodeId() const noexcept;

    /// The compiled expression (read-only).
    [[nodiscard]] const nexus::script::Expression& expression() const noexcept;

    /// The variable→sourceNode binding list supplied at construction.
    [[nodiscard]] const std::vector<ExpressionBinding>& bindings() const noexcept;

    /// Resolve variables from `ctx.inputPayloads`, evaluate the expression,
    /// and write a ScalarF32 result to `*ctx.outputPayload`.
    ///
    /// Returns false (and leaves outputPayload unchanged) when:
    ///   - `ctx.outputPayload` is null.
    ///   - A bound variable's upstream payload is absent or not ScalarF32.
    ///   - Expression evaluation returns nullopt (domain error, missing var).
    [[nodiscard]] bool compute(NodeComputeContext& ctx) const;

private:
    ExpressionNodeAdapter();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace nexus::eval
