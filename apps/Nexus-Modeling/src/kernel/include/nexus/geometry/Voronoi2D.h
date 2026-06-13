#pragma once
// --- Nexus Geometry — Voronoi2D

#include <nexus/geometry/Delaunay2D.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

struct VoronoiVertex {
    Vec2 pos;
    bool finite = true;
};

struct VoronoiEdge {
    uint32_t cellA, cellB;
    Vec2 start, end;
    bool finite = true;
};

struct VoronoiResult {
    bool ok = false;
    std::string error;
    std::vector<VoronoiVertex> vertices;
    std::vector<VoronoiEdge> edges;
};

class Voronoi2D {
public:
    [[nodiscard]] static VoronoiResult build(const DelaunayResult& delaunay);
    [[nodiscard]] static VoronoiResult compute(const std::vector<Vec2>& points);
};

} // namespace nexus::geometry
