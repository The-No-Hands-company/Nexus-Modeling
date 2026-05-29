#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Eval Extension — SubgraphTemplate serialization (Slice 3)
//
//  Text-format round-trip serializer for SubgraphTemplate.
//  Follows the same line-oriented tagged format as ParametricGraphSerializer.
//
//  Format:
//    NEXUS_SUBGRAPH_V1
//    N <localId> <kind> <name>          -- node record
//    E <srcId> <dstId>                  -- edge record
//    I <portName> <localId>             -- input port
//    O <portName> <localId>             -- output port
//    P <localId> <type> [value]         -- payload (omitted for None/unsupported)
//
//  Name and portName tokens must be non-empty and contain only:
//    alphanumeric characters, '_', '-', '.'
//  (This matches the constraint the parametric serializer implicitly assumes.)
//
//  Payload types serialized: None (omitted), ScalarF32, IntegerI64, Boolean,
//  TextUtf8 (UTF-8 token, same name restrictions apply).
//  Binary, SplatCloud, ReconstructionDiagnostic are not serialized (runtime
//  payloads); deserialization skips unknown payload type tags gracefully.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/eval_ext/Subgraph.h>

#include <string>
#include <vector>

namespace nexus::eval_ext {

struct SubgraphSerializationReport {
    bool ok = true;
    // Sorted lexicographically on every return path.
    std::vector<std::string> errors;
};

class SubgraphSerializer {
public:
    /// Serialize `tmpl` to a text string.
    /// Returns an empty string and a non-ok report when any node name or port
    /// name contains characters outside the allowed token set (alphanumeric, _-.).
    [[nodiscard]] static std::string serialize(const SubgraphTemplate& tmpl,
                                               SubgraphSerializationReport& report);

    /// Deserialize a string produced by `serialize` into `outTemplate`.
    /// `outTemplate` is cleared before population.
    /// Returns false in `report.ok` if the header is missing, any record is
    /// malformed, referenced IDs are unknown, or cycle/port validation fails.
    [[nodiscard]] static SubgraphSerializationReport deserialize(
        const std::string& data, SubgraphTemplate& outTemplate);
};

} // namespace nexus::eval_ext
