#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — half-edge mesh topology
//
//  Provides O(1) adjacency traversal for all standard mesh editing operations:
//  face/vertex star iteration, boundary loops, edge flip, edge split.
//
//  Storage: flat arrays indexed by uint32_t handles.
//  kHEInvalid (~0u) is the sentinel for absent/boundary links (e.g. twin on a
//  boundary half-edge).
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace nexus::geometry {

inline constexpr uint32_t kHEInvalid = ~0u;

// ─── Per-element records ──────────────────────────────────────────────────────

struct HalfEdgeRecord {
    uint32_t twin = kHEInvalid; // opposite half-edge (kHEInvalid on boundary)
    uint32_t next = kHEInvalid; // next half-edge around the same face (CCW)
    uint32_t prev = kHEInvalid; // previous half-edge around the same face
    uint32_t src  = kHEInvalid; // source vertex index
    uint32_t dst  = kHEInvalid; // destination vertex index
    uint32_t face = kHEInvalid; // owning face index (kHEInvalid for boundary HEs)
};

struct HEVertexRecord {
    uint32_t outgoing = kHEInvalid; // any outgoing half-edge from this vertex
    nexus::render::Vec3 position{};
};

struct HEFaceRecord {
    uint32_t halfEdge = kHEInvalid; // any half-edge on this face's boundary
};

// ─── HalfEdgeMesh ─────────────────────────────────────────────────────────────

class HalfEdgeMesh {
public:
    // Construct from an indexed polygon soup.  Returns std::nullopt if the mesh
    // contains non-manifold edges (edge shared by more than two faces) or if any
    // face has fewer than 3 vertices.
    [[nodiscard]] static std::optional<HalfEdgeMesh> fromMesh(const Mesh& mesh);

    // Export back to an indexed triangle mesh (triangulates non-tri faces by fan).
    [[nodiscard]] Mesh toMesh() const;

    // ── Topology predicates ───────────────────────────────────────────────────

    // True iff every edge has exactly one twin (no boundary edges).
    [[nodiscard]] bool isClosed() const noexcept;

    // True iff every edge has at most two incident faces (no non-manifold edges)
    // and every vertex has a single connected fan of faces.
    [[nodiscard]] bool isManifold() const noexcept;

    // True iff every face has exactly 3 half-edges.
    [[nodiscard]] bool isTriangulated() const noexcept;

    // ── Adjacency traversal ───────────────────────────────────────────────────

    // Vertex indices for a face, in half-edge traversal order.
    [[nodiscard]] std::vector<uint32_t> faceVertices(uint32_t face) const;

    // Face indices incident to a vertex (vertex star).
    [[nodiscard]] std::vector<uint32_t> vertexFaces(uint32_t vertex) const;

    // All open boundary loops as ordered vertex rings.
    [[nodiscard]] std::vector<std::vector<uint32_t>> boundaryLoops() const;

    // ── Local editing operations ──────────────────────────────────────────────

    // Flip the interior edge of the given half-edge (must be interior + triangulated).
    // Returns false if the flip is not legal (boundary edge, non-triangulated faces).
    [[nodiscard]] bool flipEdge(uint32_t he);

    // Split the edge of the given half-edge by inserting a midpoint vertex.
    // Returns the index of the new vertex, or kHEInvalid on failure.
    [[nodiscard]] uint32_t splitEdge(uint32_t he);

    // ── Raw accessors ─────────────────────────────────────────────────────────

    [[nodiscard]] const std::vector<HalfEdgeRecord>& halfEdges() const noexcept { return m_halfEdges; }
    [[nodiscard]] const std::vector<HEVertexRecord>& vertices()  const noexcept { return m_vertices; }
    [[nodiscard]] const std::vector<HEFaceRecord>&   faces()     const noexcept { return m_faces; }

    [[nodiscard]] uint32_t halfEdgeCount() const noexcept { return static_cast<uint32_t>(m_halfEdges.size()); }
    [[nodiscard]] uint32_t vertexCount()   const noexcept { return static_cast<uint32_t>(m_vertices.size()); }
    [[nodiscard]] uint32_t faceCount()     const noexcept { return static_cast<uint32_t>(m_faces.size()); }

private:
    std::vector<HalfEdgeRecord> m_halfEdges;
    std::vector<HEVertexRecord> m_vertices;
    std::vector<HEFaceRecord>   m_faces;
};

} // namespace nexus::geometry
