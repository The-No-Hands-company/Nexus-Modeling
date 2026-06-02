#include <nexus/geometry/Delaunay2D.h>
#include <nexus/geometry/RobustPredicates.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace nexus::geometry {

using predicates::inCircle;

// ─── Vertex indexing convention ──────────────────────────────────────────────
// m_allPoints = [super0, super1, super2, user0, user1, ...]
// Super-triangle vertices are always at indices 0, 1, 2.
// User point i is at m_allPoints index i + kSuperOffset.
// finalise() strips super verts and outputs triangle indices in user space.

static constexpr uint32_t kSuperOffset = 3;

// ─── Public API ──────────────────────────────────────────────────────────────

void Delaunay2D::clear() noexcept
{
    m_points.clear();
    m_allPoints.clear();
    m_tris.clear();
    m_triangles.clear();
}

uint32_t Delaunay2D::insert(double x, double y)
{
    if (m_allPoints.empty()) {
        buildSuperTriangle();
    }

    // Check for coincident point.
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_points.size()); ++i) {
        const auto& p = m_points[i];
        const double dx = p.x - x, dy = p.y - y;
        if (dx*dx + dy*dy < 1e-20) {
            return i;
        }
    }

    const uint32_t userIdx = static_cast<uint32_t>(m_points.size());
    const uint32_t allIdx  = userIdx + kSuperOffset;
    m_points.push_back({x, y});
    m_allPoints.push_back({x, y}); // appended after super verts

    insertPoint(allIdx);
    return userIdx;
}

void Delaunay2D::triangulate(const std::vector<Point2D>& points)
{
    clear();
    if (points.empty()) {
        return;
    }

    buildSuperTriangle();
    for (uint32_t i = 0; i < static_cast<uint32_t>(points.size()); ++i) {
        m_points.push_back(points[i]);
        m_allPoints.push_back(points[i]);
        insertPoint(i + kSuperOffset);
    }
    finalise();
}

Mesh Delaunay2D::toMesh() const
{
    Mesh m;
    MeshAttributes attrs;
    std::vector<nexus::render::Vec3> positions;
    positions.reserve(m_points.size());
    for (const auto& p : m_points) {
        positions.push_back({static_cast<float>(p.x), static_cast<float>(p.y), 0.f});
    }
    attrs.setPositions(std::move(positions));
    m.attributes() = std::move(attrs);
    for (const auto& tri : m_triangles) {
        Face f;
        f.indices = {tri.a, tri.b, tri.c};
        m.topology().addFace(std::move(f));
    }
    return m;
}

// ─── Internal implementation ─────────────────────────────────────────────────

void Delaunay2D::buildSuperTriangle()
{
    // Super-triangle large enough to bound any point set we'll insert.
    // Fixed at indices 0, 1, 2 in m_allPoints.
    constexpr double kBig = 1e7;
    m_allPoints.clear();
    m_allPoints.push_back({-kBig,  -kBig});
    m_allPoints.push_back({ kBig,  -kBig});
    m_allPoints.push_back({  0.0,   kBig});

    Triangle st;
    st.v[0] = 0; st.v[1] = 1; st.v[2] = 2;
    st.adj[0] = st.adj[1] = st.adj[2] = kNone;
    st.adjEdge[0] = st.adjEdge[1] = st.adjEdge[2] = 0;
    st.bad = false;
    m_tris.clear();
    m_tris.push_back(st);
}

bool Delaunay2D::inCircumcircle(uint32_t triIdx, uint32_t ptIdx) const noexcept
{
    const Triangle& t = m_tris[triIdx];
    // Use actual coordinates for all points, including super-triangle vertices.
    // The super-triangle uses large finite coordinates (±1e7) which makes its
    // circumcircle cover the entire working region, achieving the same effect
    // as the "infinite vertex" trick without the special-casing bugs.
    const double pa[2] = {m_allPoints[t.v[0]].x, m_allPoints[t.v[0]].y};
    const double pb[2] = {m_allPoints[t.v[1]].x, m_allPoints[t.v[1]].y};
    const double pc[2] = {m_allPoints[t.v[2]].x, m_allPoints[t.v[2]].y};
    const double pd[2] = {m_allPoints[ptIdx].x,   m_allPoints[ptIdx].y};
    return inCircle(pa, pb, pc, pd) > 0.0;
}

void Delaunay2D::insertPoint(uint32_t ptIdx)
{
    // Bowyer-Watson: mark all triangles whose circumcircle contains ptIdx.
    std::vector<uint32_t> bad;
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_tris.size()); ++i) {
        if (!m_tris[i].bad && inCircumcircle(i, ptIdx)) {
            m_tris[i].bad = true;
            bad.push_back(i);
        }
    }

    // Collect the boundary of the polygonal hole.
    struct BoundaryEdge {
        uint32_t v0, v1;
    };
    std::vector<BoundaryEdge> boundary;

    for (uint32_t bi : bad) {
        const Triangle& t = m_tris[bi];
        for (uint32_t e = 0; e < 3; ++e) {
            const uint32_t adjT = t.adj[e];
            if (adjT == kNone || !m_tris[adjT].bad) {
                boundary.push_back({t.v[e], t.v[(e + 1) % 3]});
            }
        }
    }

    // Create one new triangle per boundary edge, all connecting to ptIdx.
    const uint32_t firstNew = static_cast<uint32_t>(m_tris.size());
    for (const auto& be : boundary) {
        Triangle nt;
        nt.v[0] = ptIdx;
        nt.v[1] = be.v0;
        nt.v[2] = be.v1;
        nt.adj[0] = kNone;
        nt.adj[1] = kNone;
        nt.adj[2] = kNone;
        nt.adjEdge[0] = nt.adjEdge[1] = nt.adjEdge[2] = 0;
        nt.bad = false;
        m_tris.push_back(nt);
    }
    const uint32_t lastNew = static_cast<uint32_t>(m_tris.size());

    // Rebuild adjacency for all currently-live (non-bad) triangles.
    // Build a map: (minV, maxV) → (triIdx, edge)
    // Use a simple O(n²) pass over live triangles to find shared edges.
    for (uint32_t i = firstNew; i < lastNew; ++i) {
        const auto& ti = m_tris[i];
        for (uint32_t e = 0; e < 3; ++e) {
            if (m_tris[i].adj[e] != kNone) { continue; }
            const uint32_t va = ti.v[e];
            const uint32_t vb = ti.v[(e + 1) % 3];
            // Search all live triangles for the reverse edge (vb, va).
            for (uint32_t j = 0; j < lastNew; ++j) {
                if (j == i || m_tris[j].bad) { continue; }
                const auto& tj = m_tris[j];
                for (uint32_t f = 0; f < 3; ++f) {
                    if (tj.v[f] == vb && tj.v[(f + 1) % 3] == va) {
                        m_tris[i].adj[e]     = j;
                        m_tris[i].adjEdge[e] = f;
                        m_tris[j].adj[f]     = i;
                        m_tris[j].adjEdge[f] = e;
                    }
                }
            }
        }
    }
}

void Delaunay2D::finalise()
{
    m_triangles.clear();
    for (const auto& t : m_tris) {
        if (t.bad) { continue; }
        // Exclude any triangle touching a super-triangle vertex (index < kSuperOffset).
        if (t.v[0] < kSuperOffset || t.v[1] < kSuperOffset || t.v[2] < kSuperOffset) {
            continue;
        }
        // Convert from allPoints indices back to user-space indices.
        m_triangles.push_back({
            t.v[0] - kSuperOffset,
            t.v[1] - kSuperOffset,
            t.v[2] - kSuperOffset
        });
    }
}

} // namespace nexus::geometry
