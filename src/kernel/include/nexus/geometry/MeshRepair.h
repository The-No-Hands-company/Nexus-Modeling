#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — mesh repair (hole filling & non-manifold resolution)
//
//  Hole filling:
//    Detects boundary loops in the half-edge mesh and fills each hole with a
//    triangle fan from the centroid of the boundary loop.  Fan triangulation
//    is simple and robust; for large holes the output may be non-flat.
//
//  Non-manifold edge resolution:
//    Detects edges shared by more than two faces (T-junctions, overlapping
//    faces) and attempts to resolve them by splitting the offending geometry.
//    Currently reports the count; full resolution is an iterative process.
//
//  Both operations return a new (modified) Mesh rather than editing in-place,
//  keeping the repair pipeline composable.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

struct MeshRepairReport {
    uint32_t holesFilled              = 0;
    uint32_t holeBoundaryEdgeCount    = 0; // total boundary edges across all holes
    uint32_t nonManifoldEdgesFound    = 0;
    uint32_t nonManifoldEdgesResolved = 0;
    bool     inputWasClosed           = false;
    bool     outputIsClosed           = false;
    std::vector<std::string> warnings;
};

struct MeshRepairOptions {
    bool fillHoles           = true;
    bool resolveNonManifold  = true;
    uint32_t maxHoleEdges    = 0; // 0 = fill all; N = skip holes with > N boundary edges
};

class MeshRepair {
public:
    // Fill boundary holes and attempt to resolve non-manifold edges.
    // Returns the repaired mesh and a report describing what changed.
    [[nodiscard]] static std::pair<Mesh, MeshRepairReport>
    repair(const Mesh& mesh, const MeshRepairOptions& opts = {});
};

} // namespace nexus::geometry
