#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Rolling-Ball Fillet on Mesh Edges
//
//  Rounds selected edges by subdividing them and offsetting interior
//  vertices along the face-bisector normal by the fillet radius.
//  Operates on HalfEdgeMesh (triangle mesh only).
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HalfEdgeMesh.h>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

struct FilletDesc {
    float    radius   = 0.1f;   // fillet radius (constant or base for variable)
    uint32_t segments = 4;      // subdivision steps along each edge (≥1)
    bool     variableRadius = false;  // use per-edge radii from radiiMap
};

struct FilletReport {
    bool     valid       = false;
    uint32_t edgesProcessed = 0;
    uint32_t verticesAdded   = 0;
};

class MeshFilletOperation {
public:
    // Apply a constant-radius fillet to specified half-edges.
    [[nodiscard]] static FilletReport fillet(HalfEdgeMesh&                mesh,
                                              const std::vector<uint32_t>& halfEdgeIndices,
                                              const FilletDesc&           desc) noexcept;

    // Apply a variable-radius fillet with per-edge radii.
    // radiiMap: halfEdge Index → radius.  Unspecified edges use desc.radius.
    [[nodiscard]] static FilletReport filletVariable(
        HalfEdgeMesh& mesh,
        const std::vector<uint32_t>& halfEdgeIndices,
        const std::unordered_map<uint32_t, float>& radiiMap,
        const FilletDesc& desc) noexcept;
};

} // namespace nexus::geometry
