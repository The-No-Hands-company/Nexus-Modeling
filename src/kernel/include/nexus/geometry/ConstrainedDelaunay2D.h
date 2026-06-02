#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Constrained Delaunay Triangulation (CDT)
//
//  Extends Delaunay2D to honour a set of constraint edges.  Each constraint
//  edge (a, b) will appear as one or more edges in the final triangulation,
//  even if that violates the Delaunay circumcircle criterion.
//
//  Algorithm:
//    1. Build unconstrained Delaunay triangulation for all vertices.
//    2. For each constraint edge, if it is already present, keep it.
//       Otherwise walk the cavity of triangles crossing the edge, re-triangulate
//       the two polygonal pockets on each side while restoring local Delaunay
//       where the constraint allows.
//
//  Constraint edges are stored as unordered pairs — orientation does not matter.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Delaunay2D.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace nexus::geometry {

struct ConstraintEdge {
    uint32_t a = 0; // user-space vertex index (into Delaunay2D::points())
    uint32_t b = 0;
};

class ConstrainedDelaunay2D {
public:
    ConstrainedDelaunay2D() = default;

    // Add a constraint edge that must appear in the triangulation.
    // Vertices are user-space indices (as returned by insert/triangulate).
    // All vertex indices must be added before calling enforceConstraints().
    void addConstraint(uint32_t a, uint32_t b);

    // Build the unconstrained Delaunay triangulation then enforce all constraint
    // edges.  Equivalent to calling Delaunay2D::triangulate() followed by
    // enforceConstraints().
    void triangulate(const std::vector<Point2D>& points,
                     const std::vector<ConstraintEdge>& constraints);

    // Access the final constrained triangles and points.
    [[nodiscard]] const std::vector<DelaunayTriangle>& triangles() const noexcept
    {
        return m_dt.triangles();
    }
    [[nodiscard]] const std::vector<Point2D>& points() const noexcept
    {
        return m_dt.points();
    }
    [[nodiscard]] const std::vector<ConstraintEdge>& constraints() const noexcept
    {
        return m_constraints;
    }

    // Export as flat Mesh with z = 0.
    [[nodiscard]] Mesh toMesh() const { return m_dt.toMesh(); }

    // True if edge (a, b) (or (b, a)) is present as a constraint.
    [[nodiscard]] bool isConstrained(uint32_t a, uint32_t b) const noexcept;

    // True if edge (a, b) (or (b, a)) appears in the current triangulation.
    [[nodiscard]] bool edgeExists(uint32_t a, uint32_t b) const noexcept;

private:
    Delaunay2D m_dt;
    std::vector<ConstraintEdge> m_constraints;

    // Insert the constraint edge (a, b) into the already-triangulated mesh.
    void insertConstraintEdge(uint32_t a, uint32_t b);
};

} // namespace nexus::geometry
