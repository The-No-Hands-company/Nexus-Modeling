#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Eval — ExpressionNodeAdapter serialization (Slice 6)
//
//  Line-oriented text format (NEXUS_EXPR_NODE_V1):
//
//    NEXUS_EXPR_NODE_V1
//    N <name> "<source>"
//    B <variableName> <sourceNodeName>
//    …
//
//  Rules:
//   - Exactly one N record per document (the adapter node).
//   - Zero or more B records (one per variable binding).
//   - Source text is double-quoted; embedded double-quotes are escaped as \".
//   - Names and variable identifiers must not contain whitespace.
//   - Unknown record tags are silently skipped (forward-compatibility).
//
//  Round-trip contract:
//   - serialize() followed by deserialize() on the same graph topology
//     (source nodes present and named identically) reconstructs an adapter
//     with an equivalent expression and binding list.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/eval/EvalGraph.h>
#include <nexus/eval/ExpressionNode.h>

#include <optional>
#include <string>
#include <vector>

namespace nexus::eval {

struct ExpressionNodeSerializationReport {
    bool ok = true;
    std::vector<std::string> errors;
    std::size_t bindingCount = 0;
};

class ExpressionNodeSerializer {
public:
    /// Serialize `adapter` to a NEXUS_EXPR_NODE_V1 text block.
    /// `graph` is needed to resolve each binding's sourceNode to a node name.
    /// On failure (e.g. unknown sourceNode) ok=false and errors are populated;
    /// the returned string is empty.
    [[nodiscard]] static std::string serialize(
        const ExpressionNodeAdapter&        adapter,
        const EvalGraph&                    graph,
        ExpressionNodeSerializationReport&  report);

    /// Parse a NEXUS_EXPR_NODE_V1 text block and create an ExpressionNodeAdapter
    /// in `graph`. Source nodes referenced by B records are looked up in `graph`
    /// by name; they must already exist.
    /// Returns nullopt when parsing fails or compilation fails; errors are
    /// appended to `report.errors` and `report.ok` is set to false.
    [[nodiscard]] static std::optional<ExpressionNodeAdapter> deserialize(
        const std::string&                  text,
        EvalGraph&                          graph,
        ExpressionNodeSerializationReport&  report);
};

} // namespace nexus::eval
