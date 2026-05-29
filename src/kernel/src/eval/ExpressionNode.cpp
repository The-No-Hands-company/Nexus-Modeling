#include <nexus/eval/ExpressionNode.h>

#include <unordered_map>
#include <utility>

namespace nexus::eval {

// ── Impl ─────────────────────────────────────────────────────────────────────

struct ExpressionNodeAdapter::Impl {
    NodeId                         nodeId;
    nexus::script::Expression      expr;
    std::vector<ExpressionBinding> bindings;
};

// ── Construction ─────────────────────────────────────────────────────────────

ExpressionNodeAdapter::ExpressionNodeAdapter() = default;

ExpressionNodeAdapter::ExpressionNodeAdapter(ExpressionNodeAdapter&&) noexcept = default;
ExpressionNodeAdapter& ExpressionNodeAdapter::operator=(ExpressionNodeAdapter&&) noexcept = default;
ExpressionNodeAdapter::~ExpressionNodeAdapter() = default;

std::optional<ExpressionNodeAdapter> ExpressionNodeAdapter::create(
    EvalGraph&                    graph,
    std::string                   name,
    std::string_view              source,
    std::vector<ExpressionBinding> bindings)
{
    // Validate bindings before touching the graph.
    for (const auto& b : bindings) {
        if (b.variable.empty()) return std::nullopt;
        if (!graph.hasNode(b.sourceNode)) return std::nullopt;
    }

    // Compile the expression.
    auto expr = nexus::script::Expression::compile(source);
    if (!expr) return std::nullopt;

    // Allocate the node and wire edges.
    NodeId id = graph.addNode(NodeKind::Expression, std::move(name));
    for (const auto& b : bindings) {
        [[maybe_unused]] bool connected = graph.connect(b.sourceNode, id);
    }

    ExpressionNodeAdapter adapter;
    adapter.m_impl = std::make_unique<Impl>(Impl{
        id,
        std::move(*expr),
        std::move(bindings)
    });
    return adapter;
}

// ── Accessors ─────────────────────────────────────────────────────────────────

NodeId ExpressionNodeAdapter::nodeId() const noexcept {
    return m_impl ? m_impl->nodeId : kInvalidNodeId;
}

const nexus::script::Expression& ExpressionNodeAdapter::expression() const noexcept {
    return m_impl->expr;
}

const std::vector<ExpressionBinding>& ExpressionNodeAdapter::bindings() const noexcept {
    return m_impl->bindings;
}

// ── Compute ───────────────────────────────────────────────────────────────────

bool ExpressionNodeAdapter::compute(NodeComputeContext& ctx) const {
    if (!m_impl) return false;
    if (!ctx.outputPayload) return false;

    // Build a variable map from inputPayloads.
    // inputPayloads is ordered by inputNodes (deterministic per EvalGraph contract).
    // We look up each binding's sourceNode in inputPayloads.
    std::unordered_map<std::string, double> vars;
    vars.reserve(m_impl->bindings.size());

    for (const auto& b : m_impl->bindings) {
        // Find the matching input payload entry.
        const NodeInputPayload* found = nullptr;
        for (const auto& inp : ctx.inputPayloads) {
            if (inp.inputNode == b.sourceNode) {
                found = &inp;
                break;
            }
        }
        if (!found || !found->payload) return false;

        const float* f = found->payload->scalarF32();
        if (!f) return false;

        vars[b.variable] = static_cast<double>(*f);
    }

    auto result = m_impl->expr.evaluate(vars);
    if (!result) return false;

    ctx.outputPayload->value = static_cast<float>(*result);
    return true;
}

} // namespace nexus::eval
