#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — subdivision surfaces (Loop and Catmull-Clark)
//
//  Loop subdivision (triangle meshes):
//    Each triangle is split into 4 by inserting edge midpoints, then vertex
//    positions are smoothed by the Loop stencils:
//      Edge vertex:  3/8 * (v0 + v1) + 1/8 * (opp0 + opp1)
//      Vertex point: (1 − n*β) * v + β * Σ(neighbours)
//        where β = (1/n)(5/8 − (3/8 + 1/4*cos(2π/n))²) for n > 3,
//              β = 3/16 for n == 3
//
//  Catmull-Clark subdivision (quad-dominant / arbitrary polygon meshes):
//    Classic Catmull-Clark (1978) stencils:
//      Face point:  average of face vertices
//      Edge point:  average of endpoints + adjacent face points
//      Vertex point: F/n + 2E/n + (n-3)/n * V
//        where F = average of adjacent face points,
//              E = average of adjacent edge midpoints, n = valence
//
//  Both algorithms operate on a HalfEdgeMesh and return a new (refined) mesh.
//  Boundary vertices use crease rules (fixed boundary positions for C1 continuity).
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HalfEdgeMesh.h>

#include <cstdint>

namespace nexus::geometry {

struct SubdivisionOptions {
    uint32_t levels      = 1;    // number of subdivision levels to apply
    bool     smoothBoundary = false; // if true, apply smooth boundary rules; else crease
};

class SubdivisionSurface {
public:
    // Loop subdivision — input must be a triangulated mesh (isTriangulated()).
    // Returns a refined HalfEdgeMesh after `levels` subdivisions.
    // Returns std::nullopt if the input is not triangulated or fromMesh fails on
    // any intermediate result.
    [[nodiscard]] static std::optional<HalfEdgeMesh>
    loopSubdivide(const HalfEdgeMesh& mesh, const SubdivisionOptions& opts = {});

    // Catmull-Clark subdivision — works on arbitrary polygon meshes.
    // Returns a refined HalfEdgeMesh after `levels` subdivisions.
    [[nodiscard]] static std::optional<HalfEdgeMesh>
    catmullClark(const HalfEdgeMesh& mesh, const SubdivisionOptions& opts = {});
};

} // namespace nexus::geometry
