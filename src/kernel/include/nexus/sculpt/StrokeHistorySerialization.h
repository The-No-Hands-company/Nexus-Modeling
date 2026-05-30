#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sculpt — Stroke history serialization (Slice 4)
//
//  Serializes and deserializes a sequence of StrokeDelta records to a compact
//  line-oriented text format.  The serialized history can be replayed by calling
//  session.redoStroke(delta) for each deserialized StrokeDelta in order.
//
//  Format:
//    NEXUS_STROKE_HISTORY_V1
//    S <strokeId> <brushKind>          -- begins a stroke section
//    D <vertexIdx> <dx> <dy> <dz>      -- one vertex delta (repeated)
//
//  - <brushKind> is the enum name string (Draw, Smooth, Inflate, ...).
//  - <dx/dy/dz> are serialized with 9 significant digits of precision.
//  - D records must follow an S record; D records before the first S are skipped.
//  - Unknown BrushKind strings cause the entire stroke to be skipped with an error.
//  - Unknown top-level tags are silently skipped (forward-compatible).
//  - An empty history serializes to a header-only string.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/sculpt/SculptSession.h>

#include <string>
#include <vector>

namespace nexus::sculpt {

struct StrokeHistorySerializationReport {
    bool ok = true;
    std::vector<std::string> errors;   ///< Sorted lexicographically.
    std::size_t strokeCount = 0;       ///< Number of StrokeDeltas written/read.
};

class StrokeHistorySerializer {
public:
    /// Serialize a sequence of StrokeDelta records to a text archive.
    /// Invalid (id == kInvalidStrokeId) deltas in the sequence are skipped and
    /// counted as errors. Returns empty string + non-ok report on any error.
    [[nodiscard]] static std::string serialize(
        const std::vector<StrokeDelta>& history,
        StrokeHistorySerializationReport& report);

    /// Deserialize a text archive into a sequence of StrokeDelta records.
    /// `outHistory` is cleared before population.
    /// Returns non-ok when the header is missing, any S/D record is malformed,
    /// or a BrushKind string is unrecognized (that stroke is skipped and an
    /// error is appended; parsing of remaining strokes continues).
    [[nodiscard]] static StrokeHistorySerializationReport deserialize(
        const std::string& data, std::vector<StrokeDelta>& outHistory);
};

} // namespace nexus::sculpt
