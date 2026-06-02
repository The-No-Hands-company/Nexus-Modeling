#include <nexus/geometry/Voronoi2D.h>

#include <cmath>
#include <map>
#include <utility>

namespace nexus::geometry {

// ─── Circumcenter ─────────────────────────────────────────────────────────────

static bool circumcenter(
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    double& outX, double& outY) noexcept
{
    // Solve for the circumcenter of triangle (a, b, c).
    const double D = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(D) < 1e-12) { return false; }
    const double a2 = ax * ax + ay * ay;
    const double b2 = bx * bx + by * by;
    const double c2 = cx * cx + cy * cy;
    outX = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / D;
    outY = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / D;
    return true;
}

// ─── fromDelaunay ─────────────────────────────────────────────────────────────

VoronoiDiagram Voronoi2D::fromDelaunay(const Delaunay2D& dt)
{
    VoronoiDiagram vd;

    const auto& pts  = dt.points();
    const auto& tris = dt.triangles();
    const uint32_t nTris = static_cast<uint32_t>(tris.size());
    const uint32_t nPts  = static_cast<uint32_t>(pts.size());

    if (nTris == 0 || nPts == 0) { return vd; }

    // Step 1: one Voronoi vertex per Delaunay triangle (its circumcenter).
    vd.m_vertices.reserve(nTris);
    for (const auto& t : tris) {
        double cx = 0.0, cy = 0.0;
        circumcenter(
            pts[t.a].x, pts[t.a].y,
            pts[t.b].x, pts[t.b].y,
            pts[t.c].x, pts[t.c].y,
            cx, cy);
        vd.m_vertices.push_back({cx, cy});
    }

    // Step 2: build adjacency — for each directed Delaunay edge (u, v) find the
    // two triangles that share it (if any).  Store undirected pairs (min, max).
    // Map: (min(u,v), max(u,v)) → list of triangle indices sharing that edge.
    using EdgeKey = std::pair<uint32_t, uint32_t>;
    std::map<EdgeKey, std::vector<uint32_t>> edgeToTris;
    for (uint32_t ti = 0; ti < nTris; ++ti) {
        const auto& t = tris[ti];
        const uint32_t v[3] = {t.a, t.b, t.c};
        for (uint32_t e = 0; e < 3; ++e) {
            uint32_t va = v[e], vb = v[(e + 1) % 3];
            if (va > vb) { std::swap(va, vb); }
            edgeToTris[{va, vb}].push_back(ti);
        }
    }

    // Step 3: one Voronoi edge per Delaunay edge.
    vd.m_cells.resize(nPts);
    for (uint32_t i = 0; i < nPts; ++i) { vd.m_cells[i].siteIndex = i; }

    for (auto& [ekey, triList] : edgeToTris) {
        const uint32_t ua = ekey.first, ub = ekey.second;

        VoronoiEdge ve;
        ve.site0 = ua;
        ve.site1 = ub;

        if (triList.size() == 2) {
            ve.v0      = triList[0];
            ve.v1      = triList[1];
            ve.isFinite = true;
        } else {
            // Boundary Delaunay edge — one finite circumcenter, ray to infinity.
            ve.v0      = triList[0];
            ve.v1      = kVoronoiInfinite;
            ve.isFinite = false;
        }

        const uint32_t edgeIdx = static_cast<uint32_t>(vd.m_edges.size());
        vd.m_edges.push_back(ve);

        if (ua < nPts) { vd.m_cells[ua].edgeIndices.push_back(edgeIdx); }
        if (ub < nPts) { vd.m_cells[ub].edgeIndices.push_back(edgeIdx); }
    }

    return vd;
}

} // namespace nexus::geometry
