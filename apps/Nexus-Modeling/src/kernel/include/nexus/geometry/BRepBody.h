#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Boundary Representation (B-Rep)
//
//  A proper vertex-edge-face-loop-shell body built on top of the half-edge
//  mesh.  Provides typed entity access, topological queries, construction
//  from closed manifold meshes, and round-trip export.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

// ──────────── Entity IDs ────────────────────────────────────────────────────

using BRepVertexId = uint32_t;
using BRepEdgeId   = uint32_t;
using BRepFaceId   = uint32_t;
using BRepShellId  = uint32_t;

inline constexpr BRepVertexId kInvalidBRepVertex = 0xFFFFFFFFu;
inline constexpr BRepEdgeId   kInvalidBRepEdge   = 0xFFFFFFFFu;
inline constexpr BRepFaceId   kInvalidBRepFace   = 0xFFFFFFFFu;
inline constexpr BRepShellId  kInvalidBRepShell  = 0xFFFFFFFFu;

// ──────────── Entity structs ────────────────────────────────────────────────

struct BRepVertex {
    BRepVertexId       id       = kInvalidBRepVertex;
    nexus::render::Vec3 point{};
    uint32_t           halfEdge = HalfEdgeMesh::kInvalid;  // one outgoing half-edge
};

struct BRepEdge {
    BRepEdgeId   id      = kInvalidBRepEdge;
    uint32_t     halfEdge = HalfEdgeMesh::kInvalid;  // one directed half-edge
    BRepVertexId v0      = kInvalidBRepVertex;
    BRepVertexId v1      = kInvalidBRepVertex;
};

struct BRepFace {
    BRepFaceId   id       = kInvalidBRepFace;
    uint32_t     halfEdge = HalfEdgeMesh::kInvalid;  // one half-edge in the outer loop
    BRepShellId  shell    = kInvalidBRepShell;
};

struct BRepShell {
    BRepShellId            id     = kInvalidBRepShell;
    std::vector<BRepFaceId> faces;
    bool                    closed = false;
};

// ──────────── Diagnostics ───────────────────────────────────────────────────

struct BRepReport {
    bool valid = false;
    std::vector<std::string> errors;
};

// ──────────── BRepBody ──────────────────────────────────────────────────────

class BRepBody {
public:
    BRepBody() = default;

    // ── Construction ─────────────────────────────────────────────────
    [[nodiscard]] static std::pair<BRepBody, BRepReport> fromMesh(const Mesh& mesh) noexcept;

    // ── Export ───────────────────────────────────────────────────────
    [[nodiscard]] Mesh toMesh() const;

    // ── Queries ──────────────────────────────────────────────────────
    [[nodiscard]] bool isManifold() const noexcept;
    [[nodiscard]] bool isClosed()   const noexcept;
    [[nodiscard]] int32_t eulerCharacteristic() const noexcept;
    [[nodiscard]] int32_t genus() const noexcept;

    // ── Counts ───────────────────────────────────────────────────────
    [[nodiscard]] size_t vertexCount() const noexcept { return m_vertices.size(); }
    [[nodiscard]] size_t edgeCount()   const noexcept { return m_edges.size(); }
    [[nodiscard]] size_t faceCount()   const noexcept { return m_faces.size(); }
    [[nodiscard]] size_t shellCount()  const noexcept { return m_shells.size(); }

    // ── Raw entity access ────────────────────────────────────────────
    [[nodiscard]] const BRepVertex& vertex(BRepVertexId id) const noexcept;
    [[nodiscard]] const BRepEdge&   edge(BRepEdgeId id) const noexcept;
    [[nodiscard]] const BRepFace&   face(BRepFaceId id) const noexcept;
    [[nodiscard]] const BRepShell&  shell(BRepShellId id) const noexcept;

    [[nodiscard]] std::span<const BRepVertex> vertices() const noexcept { return m_vertices; }
    [[nodiscard]] std::span<const BRepEdge>   edges()    const noexcept { return m_edges; }
    [[nodiscard]] std::span<const BRepFace>   faces()    const noexcept { return m_faces; }
    [[nodiscard]] std::span<const BRepShell>  shells()   const noexcept { return m_shells; }

    // ── Adjacency traversal ──────────────────────────────────────────
    // Faces incident to a vertex.
    [[nodiscard]] std::vector<BRepFaceId> vertexFaces(BRepVertexId v) const noexcept;
    // Faces sharing an edge.
    [[nodiscard]] std::vector<BRepFaceId> edgeFaces(BRepEdgeId e) const noexcept;
    // Edges bounding a face.
    [[nodiscard]] std::vector<BRepEdgeId> faceEdges(BRepFaceId f) const noexcept;
    // Vertices of a face (CCW order).
    [[nodiscard]] std::vector<BRepVertexId> faceVertices(BRepFaceId f) const noexcept;

    // ── Half-edge access ─────────────────────────────────────────────
    [[nodiscard]] const HalfEdgeMesh& halfEdgeMesh() const noexcept { return m_heMesh; }

private:
    HalfEdgeMesh           m_heMesh;
    std::vector<BRepVertex> m_vertices;
    std::vector<BRepEdge>   m_edges;
    std::vector<BRepFace>   m_faces;
    std::vector<BRepShell>  m_shells;

    // Maps a half-edge index to its owning BRepEdge for O(1) lookup.
    std::unordered_map<uint32_t, BRepEdgeId> m_halfEdgeToEdge;
};

} // namespace nexus::geometry
