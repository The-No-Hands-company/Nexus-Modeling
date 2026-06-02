#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — 2-D Voronoi diagram (dual of Delaunay)
//
//  Computes the Voronoi diagram as the geometric dual of a Delaunay
//  triangulation.  Each Delaunay triangle maps to a Voronoi vertex at its
//  circumcenter; each Delaunay edge maps to a Voronoi edge connecting the
//  circumcenters of its two incident triangles (or a half-infinite ray for
//  boundary Delaunay edges).
//
//  Only finite Voronoi vertices are emitted; rays for boundary edges are
//  represented as edges with one endpoint index == kVoronoiInfinite.
//
//  Usage:
//    Delaunay2D dt;
//    dt.triangulate(pts);
//    auto vd = Voronoi2D::fromDelaunay(dt);
//    for (auto& cell : vd.cells())  { /* cell.siteIndex, cell.edgeIndices */ }
//    for (auto& edge : vd.edges())  { /* edge.v0, edge.v1 (vertex indices) */ }
//    for (auto& vert : vd.vertices()) { /* vert.x, vert.y */ }
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Delaunay2D.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

inline constexpr uint32_t kVoronoiInfinite = ~0u; // sentinel for a ray endpoint

struct VoronoiVertex {
    double x = 0.0;
    double y = 0.0;
};

struct VoronoiEdge {
    uint32_t v0 = kVoronoiInfinite; // index into VoronoiDiagram::vertices()
    uint32_t v1 = kVoronoiInfinite;
    uint32_t site0 = 0; // Delaunay site (input point index) on one side
    uint32_t site1 = 0; // Delaunay site on the other side
    bool isFinite = false; // false if either endpoint is a ray
};

struct VoronoiCell {
    uint32_t siteIndex = 0; // input point index
    std::vector<uint32_t> edgeIndices; // indices into VoronoiDiagram::edges()
};

class VoronoiDiagram {
public:
    [[nodiscard]] const std::vector<VoronoiVertex>& vertices() const noexcept { return m_vertices; }
    [[nodiscard]] const std::vector<VoronoiEdge>&   edges()    const noexcept { return m_edges; }
    [[nodiscard]] const std::vector<VoronoiCell>&   cells()    const noexcept { return m_cells; }

    [[nodiscard]] uint32_t vertexCount() const noexcept { return static_cast<uint32_t>(m_vertices.size()); }
    [[nodiscard]] uint32_t edgeCount()   const noexcept { return static_cast<uint32_t>(m_edges.size()); }
    [[nodiscard]] uint32_t cellCount()   const noexcept { return static_cast<uint32_t>(m_cells.size()); }

private:
    friend class Voronoi2D;
    std::vector<VoronoiVertex> m_vertices;
    std::vector<VoronoiEdge>   m_edges;
    std::vector<VoronoiCell>   m_cells;
};

class Voronoi2D {
public:
    // Build the Voronoi diagram as the dual of an already-triangulated Delaunay mesh.
    // The Delaunay triangulation must have been finalised (triangulate() called).
    [[nodiscard]] static VoronoiDiagram fromDelaunay(const Delaunay2D& dt);
};

} // namespace nexus::geometry
