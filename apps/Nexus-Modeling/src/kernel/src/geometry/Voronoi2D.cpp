#include <nexus/geometry/Voronoi2D.h>
#include <nexus/geometry/RobustPredicates.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace nexus::geometry {

namespace {

// circumcenter of triangle (a, b, c)
Vec2 circumcenter(const Vec2& a, const Vec2& b, const Vec2& c) {
    double d = 2.0 * (a.u * (b.v - c.v) + b.u * (c.v - a.v) + c.u * (a.v - b.v));
    if (std::abs(d) < 1e-20) {
        double cx = (a.u + b.u + c.u) / 3.0;
        double cy = (a.v + b.v + c.v) / 3.0;
        return {static_cast<float>(cx), static_cast<float>(cy)};
    }
    double ux = ((a.u * a.u + a.v * a.v) * (b.v - c.v) +
                 (b.u * b.u + b.v * b.v) * (c.v - a.v) +
                 (c.u * c.u + c.v * c.v) * (a.v - b.v)) / d;
    double uy = ((a.u * a.u + a.v * a.v) * (c.u - b.u) +
                 (b.u * b.u + b.v * b.v) * (a.u - c.u) +
                 (c.u * c.u + c.v * c.v) * (b.u - a.u)) / d;
    return {static_cast<float>(ux), static_cast<float>(uy)};
}

struct EdgeKey {
    uint32_t lo, hi;
    EdgeKey(uint32_t a, uint32_t b) : lo(std::min(a, b)), hi(std::max(a, b)) {}
    bool operator==(const EdgeKey& o) const { return lo == o.lo && hi == o.hi; }
};

struct EdgeKeyHash {
    size_t operator()(const EdgeKey& k) const {
        return static_cast<size_t>((static_cast<uint64_t>(k.hi) << 32) | k.lo);
    }
};

} // anonymous namespace

// --- build --------------------------------------------------------------------

VoronoiResult Voronoi2D::build(const DelaunayResult& delaunay) {
    VoronoiResult result;

    if (!delaunay.ok || delaunay.triangles.empty()) {
        result.ok = false;
        result.error = "Invalid Delaunay input";
        return result;
    }

    const auto& tris = delaunay.triangles;
    const auto& verts = delaunay.vertices;

    // --- Build edge -> triangle adjacency map ---------------------------------
    // Each Delaunay edge is shared by 1 or 2 triangles.
    std::unordered_map<EdgeKey, std::vector<size_t>, EdgeKeyHash> edgeTris;

    for (size_t ti = 0; ti < tris.size(); ++ti) {
        const auto& t = tris[ti];
        uint32_t es[3][2] = {{t[0], t[1]}, {t[1], t[2]}, {t[2], t[0]}};
        for (int e = 0; e < 3; ++e) {
            EdgeKey key(es[e][0], es[e][1]);
            edgeTris[key].push_back(ti);
        }
    }

    // --- Compute circumcenters (Voronoi vertices) for each triangle -----------
    std::unordered_map<size_t, uint32_t> triToVoronoiVert; // triangle index -> voronoi vertex index
    for (size_t ti = 0; ti < tris.size(); ++ti) {
        const auto& t = tris[ti];
        if (t[0] >= verts.size() || t[1] >= verts.size() || t[2] >= verts.size()) continue;
        Vec2 cc = circumcenter(verts[t[0]], verts[t[1]], verts[t[2]]);
        uint32_t vi = static_cast<uint32_t>(result.vertices.size());
        result.vertices.push_back({cc, true});
        triToVoronoiVert[ti] = vi;
    }

    // --- Build Voronoi edges from Delaunay edges ------------------------------
    std::unordered_set<EdgeKey, EdgeKeyHash> emittedEdges;

    for (auto& [ekey, adjTris] : edgeTris) {
        if (adjTris.empty()) continue;

        if (adjTris.size() == 2) {
            // Interior edge: connect two circumcenters
            size_t ta = adjTris[0];
            size_t tb = adjTris[1];
            auto itA = triToVoronoiVert.find(ta);
            auto itB = triToVoronoiVert.find(tb);
            if (itA == triToVoronoiVert.end() || itB == triToVoronoiVert.end()) continue;

            VoronoiEdge ve;
            ve.cellA = ekey.lo;
            ve.cellB = ekey.hi;
            ve.start = result.vertices[itA->second].pos;
            ve.end = result.vertices[itB->second].pos;
            ve.finite = true;
            result.edges.push_back(ve);
        } else if (adjTris.size() == 1) {
            // Boundary edge: extend from circumcenter to "infinity"
            // Direction: perpendicular to the Delaunay edge, pointing outward
            size_t ta = adjTris[0];
            auto itA = triToVoronoiVert.find(ta);
            if (itA == triToVoronoiVert.end()) continue;

            Vec2 edgeDir = {
                verts[ekey.hi].u - verts[ekey.lo].u,
                verts[ekey.hi].v - verts[ekey.lo].v
            };
            float elen = std::sqrt(edgeDir.u * edgeDir.u + edgeDir.v * edgeDir.v);
            if (elen < 1e-10f) continue;

            // Perpendicular (rotate 90° CCW), normalize and extend
            Vec2 perp = {-edgeDir.v / elen, edgeDir.u / elen};

            // Determine which direction points outward (away from the triangle interior)
            Vec2 mid = {
                (verts[ekey.lo].u + verts[ekey.hi].u) * 0.5f,
                (verts[ekey.lo].v + verts[ekey.hi].v) * 0.5f
            };
            Vec2 cc = result.vertices[itA->second].pos;
            Vec2 toCC = {cc.u - mid.u, cc.v - mid.v};

            if (perp.u * toCC.u + perp.v * toCC.v > 0) {
                perp = {-perp.u, -perp.v};
            }

            float far = 1e6f;
            Vec2 farPt = {cc.u + perp.u * far, cc.v + perp.v * far};

            VoronoiEdge ve;
            ve.cellA = ekey.lo;
            ve.cellB = ekey.hi;
            ve.start = cc;
            ve.end = farPt;
            ve.finite = false;
            result.edges.push_back(ve);
        }
        // adjTris.size() > 2 is degenerate; skip
    }

    result.ok = true;
    return result;
}

// --- compute ------------------------------------------------------------------

VoronoiResult Voronoi2D::compute(const std::vector<Vec2>& points) {
    auto dt = Delaunay2D::triangulate(points);
    if (!dt.ok) {
        VoronoiResult r;
        r.error = dt.error;
        return r;
    }
    return build(dt);
}

} // namespace nexus::geometry
