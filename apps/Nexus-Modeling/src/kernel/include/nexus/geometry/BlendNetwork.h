#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Blend Network, Topology Healing, Sheet Metal
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// ── Blend Network Solver ──────────────────────────────────────────────────

struct BlendEdge {
    uint32_t halfEdge;
    float radius;
};

struct BlendCorner {
    uint32_t vertex;                    // vertex where 3+ edges meet
    std::vector<uint32_t> incidentEdges; // edges entering this vertex
    float radius;                        // corner blend radius
};

struct BlendNetworkReport {
    Mesh result;
    uint32_t edgesProcessed = 0;
    uint32_t cornersProcessed = 0;
    bool allConverged = false;
};

// Solve a network of intersecting blends: apply edge fillets, then
// resolve corner intersections where three or more fillets meet.
[[nodiscard]] BlendNetworkReport solveBlendNetwork(
    const HalfEdgeMesh& mesh,
    const std::vector<BlendEdge>& edges,
    const std::vector<BlendCorner>& corners = {}) noexcept;

// ── Topology Healing ──────────────────────────────────────────────────────

struct HealOptions {
    float weldTolerance = 1e-5f;        // merge vertices within this distance
    float minFaceArea = 1e-8f;          // remove faces smaller than this
    float maxGapSize = 0.1f;           // close boundary gaps up to this size
    bool fixOrientation = true;         // make all face normals consistent
    bool removeNonManifold = true;      // attempt to fix non-manifold edges
    bool fillSmallHoles = true;         // fill boundary loops with few edges
    uint32_t maxHoleEdges = 8;          // holes with ≤ this many edges get filled
};

struct HealReport {
    Mesh result;
    uint32_t verticesWelded = 0;
    uint32_t facesRemoved = 0;
    uint32_t holesFilled = 0;
    uint32_t orientationsFixed = 0;
    uint32_t nonManifoldFixed = 0;
    bool complete = false;
};

// Comprehensive topology healing: weld, remove degenerates, close gaps,
// fix orientations, resolve non-manifold edges, fill small holes.
[[nodiscard]] HealReport healTopology(
    Mesh& mesh, const HealOptions& opts = {}) noexcept;

// ── Sheet Metal Unfolding ─────────────────────────────────────────────────

struct BendLine {
    uint32_t edgeIndex;         // half-edge index of the bend
    float angleDeg;             // bend angle (0 = flat, 90 = right angle)
    float radius;               // inside bend radius
    float kFactor = 0.44f;      // neutral axis offset (0-1)
};

struct UnfoldResult {
    Mesh flatPattern;
    float developedLength = 0.f;
    float bendAllowance = 0.f;
    std::vector<Vec3> bendLines; // 2D positions of bend lines on flat pattern
};

// Compute the flat pattern of a sheet metal part.
// Uses K-factor bend allowance: BA = π/180 × (R + K×T) × A
// where R = inside radius, T = thickness, A = bend angle, K = K-factor.
[[nodiscard]] UnfoldResult unfoldSheetMetal(
    const HalfEdgeMesh& bentPart,
    const std::vector<BendLine>& bends,
    float materialThickness) noexcept;

// ── Replace Face (Full Implementation) ────────────────────────────────────

struct ReplaceFaceResult {
    std::optional<Mesh> mesh;
    bool success = false;
    std::vector<uint32_t> affectedFaces; // faces that were retriangulated
};

// Replace a face with a new surface.  Trims all adjacent faces to the
// new surface boundary, extending or shrinking edges as needed.
[[nodiscard]] ReplaceFaceResult replaceFaceWithSurface(
    const HalfEdgeMesh& mesh,
    uint32_t faceIndex,
    const NurbsSurface& newSurface) noexcept;

} // namespace nexus::geometry
