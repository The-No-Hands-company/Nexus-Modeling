#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — mesh solid topology analysis
//
//  Computes standard topology descriptors from a half-edge mesh in a single
//  traversal.  All descriptors are derived from the Euler formula:
//
//    χ = V − E + F   (Euler characteristic)
//
//  For a closed orientable surface:
//    genus = (2 − χ) / 2
//
//  For an open surface with b boundary loops:
//    genus = (2 − χ − b) / 2   (if the surface is orientable)
//
//  These are the standard descriptors reported by production solid modelers
//  (CATIA, Parasolid, OpenCASCADE) before topological surgery.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HalfEdgeMesh.h>
#include <cstdint>

namespace nexus::geometry {

struct MeshTopologyAnalysis {
    uint32_t vertexCount       = 0;
    uint32_t edgeCount         = 0; // undirected edges = halfEdgeCount / 2 (approx for open)
    uint32_t faceCount         = 0;
    int32_t  eulerCharacteristic = 0; // χ = V − E + F
    int32_t  genus             = -1; // (2 − χ) / 2 for closed; −1 if not applicable
    uint32_t connectedComponents = 0;
    uint32_t boundaryLoopCount = 0;
    bool     isManifold        = false;
    bool     isClosed          = false;
    bool     isTriangulated    = false;
};

class MeshTopologyAnalyser {
public:
    // Computes all topology descriptors in a single traversal of the half-edge mesh.
    [[nodiscard]] static MeshTopologyAnalysis analyse(const HalfEdgeMesh& mesh) noexcept;
};

} // namespace nexus::geometry
