#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  NodeScene serialization (Slice 5)
//
//  Line-oriented text archive for a NodeScene snapshot.
//
//  Format: NEXUS_NODE_SCENE_V1
//    N <id> <kindString> <name>
//    E <srcId> <dstId>
//    P <childId> <parentId>
//    A <id> <typeString> <value…>
//
//  Supported asset payload types: ScalarF32, IntegerI64, Boolean,
//  ReconstructionDiagnostic (<residual> <confidence>),
//  SimTransform (<px> <py> <pz> <vx> <vy> <vz>).
//  Binary, SplatCloud, and TextUtf8 payloads are not serialized (skipped).
//
//  Node names must be valid tokens: [a-zA-Z0-9_\-.] (no spaces).
//  Unknown top-level tags are silently skipped (forward-compatible).
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/scene/NodeScene.h>

#include <string>
#include <vector>

namespace nexus {

struct NodeSceneSerializationReport {
    bool ok = true;
    std::vector<std::string> errors;
    std::size_t nodeCount = 0;
    std::size_t edgeCount = 0;
};

class NodeSceneSerializer {
public:
    /// Serialize `scene` to a line-oriented text archive.
    /// Returns an empty string and sets report.ok=false on any error.
    [[nodiscard]] static std::string serialize(
        const NodeScene& scene,
        NodeSceneSerializationReport& report);

    /// Deserialize a text archive into `scene` (which must be empty).
    /// Returns the report; scene is populated for every successfully
    /// parsed node. Unknown/unsupported tags are silently skipped.
    [[nodiscard]] static NodeSceneSerializationReport deserialize(
        const std::string& data,
        NodeScene& scene);
};

} // namespace nexus
