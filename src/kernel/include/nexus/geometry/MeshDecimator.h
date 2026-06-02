#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — QEM mesh decimation (Garland-Heckbert 1997)
//
//  Progressively collapses edges ordered by quadric error until a target face
//  or vertex count is reached.  Each vertex stores a 4×4 symmetric quadric Q
//  (sum of plane-equation outer products for all incident faces).  For each
//  candidate edge (v0, v1):
//    Q_edge = Q_v0 + Q_v1
//    error  = v̄ᵀ Q_edge v̄   (v̄ = optimal collapse position)
//  Collapses are applied in ascending error order via a priority queue.
//
//  The implementation operates on a HalfEdgeMesh and returns a decimated mesh.
//  The link-condition check from collapseEdge() prevents non-manifold output.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HalfEdgeMesh.h>

#include <cstdint>
#include <optional>

namespace nexus::geometry {

struct DecimationOptions {
    uint32_t targetFaceCount  = 0;    // stop when face count ≤ this; 0 = use targetRatio
    float    targetRatio      = 0.5f; // fraction of original faces to keep (if targetFaceCount==0)
    float    maxError         = 1e30f; // never collapse an edge with error above this
    bool     preserveBoundary = true;  // refuse to collapse boundary edges
};

struct DecimationReport {
    uint32_t facesIn   = 0;
    uint32_t facesOut  = 0;
    uint32_t collapses = 0;
    float    maxErrorApplied = 0.0f;
};

class MeshDecimator {
public:
    // Decimate mesh using Garland-Heckbert QEM.
    // Returns the decimated HalfEdgeMesh and a report, or std::nullopt if the
    // input mesh is invalid (non-manifold / empty).
    [[nodiscard]] static std::optional<std::pair<HalfEdgeMesh, DecimationReport>>
    decimate(const HalfEdgeMesh& mesh, const DecimationOptions& opts = {});
};

} // namespace nexus::geometry
