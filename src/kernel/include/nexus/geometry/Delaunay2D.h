#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — 2-D Delaunay triangulation (Bowyer-Watson)
//
//  Builds an exact Delaunay triangulation using the `inCircle` predicate from
//  RobustPredicates.  Every triangle in the output satisfies the Delaunay
//  criterion: no input point lies strictly inside the circumcircle of any
//  triangle.
//
//  Usage:
//    Delaunay2D dt;
//    dt.triangulate({{0,0},{1,0},{0,1},{1,1}});
//    for (auto [a,b,c] : dt.triangles()) { ... }
//    Mesh m = dt.toMesh();  // z = 0
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <array>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

// Index triple for one output triangle.
struct DelaunayTriangle {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
};

class Delaunay2D {
public:
    Delaunay2D() = default;

    // Remove all points and triangles.
    void clear() noexcept;

    // Insert a single point into the incremental triangulation.
    // Returns the index of the inserted point (or an existing coincident point).
    uint32_t insert(double x, double y);

    // Bulk-insert all points from a span and build the complete triangulation.
    // Equivalent to calling insert() for each point followed by finalise().
    void triangulate(const std::vector<Point2D>& points);

    // Access the output triangles (indices into the inserted point array).
    // The super-triangle vertices are excluded; indices refer to user points only.
    [[nodiscard]] const std::vector<DelaunayTriangle>& triangles() const noexcept
    {
        return m_triangles;
    }

    // Access the inserted point set (user points only, super-triangle excluded).
    [[nodiscard]] const std::vector<Point2D>& points() const noexcept { return m_points; }

    // Export as a flat Mesh with z = 0.
    [[nodiscard]] Mesh toMesh() const;

private:
    // Internal triangle storage (includes super-triangle vertices during construction).
    struct Triangle {
        uint32_t v[3];       // vertex indices (into m_allPoints)
        uint32_t adj[3];     // adjacent triangle indices (kNone if boundary)
        uint32_t adjEdge[3]; // which edge of adj[i] borders this triangle at edge i
        bool bad = false;    // marked for removal during insertion
    };

    static constexpr uint32_t kNone = ~0u;

    std::vector<Point2D>    m_points;    // user points (indices 0..n-1)
    std::vector<Point2D>    m_allPoints; // user + 3 super-triangle points
    std::vector<Triangle>   m_tris;      // working triangle set
    std::vector<DelaunayTriangle> m_triangles; // final output

    void buildSuperTriangle();
    void insertPoint(uint32_t ptIdx);
    void finalise();     // remove super-triangle, fill m_triangles

    // Returns triangle index containing point pt (walk from triangle 0).
    uint32_t findContaining(uint32_t ptIdx) const noexcept;

    // Returns true if triangle t contains point at index ptIdx strictly.
    bool inCircumcircle(uint32_t triIdx, uint32_t ptIdx) const noexcept;
};

} // namespace nexus::geometry
