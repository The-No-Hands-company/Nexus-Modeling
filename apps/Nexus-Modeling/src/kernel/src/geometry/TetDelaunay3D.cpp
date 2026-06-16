#include <nexus/geometry/TetDelaunay3D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

struct Tet {
    std::array<uint32_t, 4> v;
    Vec3 center;
    float radiusSq = 0.f;

    bool isValid() const noexcept {
        return v[0] != v[1] && v[0] != v[2] && v[0] != v[3]
            && v[1] != v[2] && v[1] != v[3]
            && v[2] != v[3];
    }
};

// Solve a 3x3 linear system Ax = b using Cramer's rule.
// Returns false if singular.
bool solve3x3(const float A[3][3], const float b[3], float x[3]) noexcept
{
    float det = A[0][0] * (A[1][1]*A[2][2] - A[1][2]*A[2][1])
              - A[0][1] * (A[1][0]*A[2][2] - A[1][2]*A[2][0])
              + A[0][2] * (A[1][0]*A[2][1] - A[1][1]*A[2][0]);
    if (std::fabs(det) < 1e-20f) return false;
    float invDet = 1.f / det;

    float A0[3][3] = {{b[0], A[0][1], A[0][2]}, {b[1], A[1][1], A[1][2]}, {b[2], A[2][1], A[2][2]}};
    float det0 = A0[0][0]*(A0[1][1]*A0[2][2]-A0[1][2]*A0[2][1])
               - A0[0][1]*(A0[1][0]*A0[2][2]-A0[1][2]*A0[2][0])
               + A0[0][2]*(A0[1][0]*A0[2][1]-A0[1][1]*A0[2][0]);
    x[0] = det0 * invDet;

    float A1[3][3] = {{A[0][0], b[0], A[0][2]}, {A[1][0], b[1], A[1][2]}, {A[2][0], b[2], A[2][2]}};
    float det1 = A1[0][0]*(A1[1][1]*A1[2][2]-A1[1][2]*A1[2][1])
               - A1[0][1]*(A1[1][0]*A1[2][2]-A1[1][2]*A1[2][0])
               + A1[0][2]*(A1[1][0]*A1[2][1]-A1[1][1]*A1[2][0]);
    x[1] = det1 * invDet;

    float A2[3][3] = {{A[0][0], A[0][1], b[0]}, {A[1][0], A[1][1], b[1]}, {A[2][0], A[2][1], b[2]}};
    float det2 = A2[0][0]*(A2[1][1]*A2[2][2]-A2[1][2]*A2[2][1])
               - A2[0][1]*(A2[1][0]*A2[2][2]-A2[1][2]*A2[2][0])
               + A2[0][2]*(A2[1][0]*A2[2][1]-A2[1][1]*A2[2][0]);
    x[2] = det2 * invDet;
    return true;
}

// Compute circumsphere of tetrahedron (a,b,c,d).
// Returns center and squared radius. Fills tet.center and tet.radiusSq.
bool computeCircumsphere(Tet& tet, const std::vector<Vec3>& pts) noexcept
{
    const Vec3& a = pts[tet.v[0]];
    const Vec3& b = pts[tet.v[1]];
    const Vec3& c = pts[tet.v[2]];
    const Vec3& d = pts[tet.v[3]];

    // Circumcenter O satisfies |O - a|^2 = |O - b|^2 = |O - c|^2 = |O - d|^2.
    // Expanding: 2*(b-a)·O = |b|^2 - |a|^2, etc.
    // Form the 3x3 system: 2*[b-a; c-a; d-a] * O = [|b|^2-|a|^2; |c|^2-|a|^2; |d|^2-|a|^2]
    float A[3][3] = {
        {2.f*(b.x - a.x), 2.f*(b.y - a.y), 2.f*(b.z - a.z)},
        {2.f*(c.x - a.x), 2.f*(c.y - a.y), 2.f*(c.z - a.z)},
        {2.f*(d.x - a.x), 2.f*(d.y - a.y), 2.f*(d.z - a.z)}
    };
    float a2 = a.x*a.x + a.y*a.y + a.z*a.z;
    float B[3] = {
        b.x*b.x + b.y*b.y + b.z*b.z - a2,
        c.x*c.x + c.y*c.y + c.z*c.z - a2,
        d.x*d.x + d.y*d.y + d.z*d.z - a2
    };

    float O[3];
    if (!solve3x3(A, B, O)) return false;

    tet.center = {O[0], O[1], O[2]};
    float dx = tet.center.x - a.x;
    float dy = tet.center.y - a.y;
    float dz = tet.center.z - a.z;
    tet.radiusSq = dx*dx + dy*dy + dz*dz;
    return true;
}

// Check if point p is inside the circumsphere of tet.
bool pointInCircumsphere(const Vec3& p, const Tet& tet, float eps) noexcept
{
    float dx = p.x - tet.center.x;
    float dy = p.y - tet.center.y;
    float dz = p.z - tet.center.z;
    float distSq = dx*dx + dy*dy + dz*dz;
    return distSq <= tet.radiusSq + eps;
}

// Hash for sorted triangle (3 vertex indices).
uint64_t hashTriangle(uint32_t a, uint32_t b, uint32_t c) noexcept
{
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
    return (static_cast<uint64_t>(a) << 40) | (static_cast<uint64_t>(b) << 20) | static_cast<uint64_t>(c);
}

struct FaceKey {
    std::array<uint32_t, 3> v;
    bool operator==(const FaceKey& o) const noexcept { return v == o.v; }
};
struct FaceKeyHash {
    uint64_t operator()(const FaceKey& k) const noexcept {
        return hashTriangle(k.v[0], k.v[1], k.v[2]);
    }
};

} // namespace

TetMesh TetDelaunay3D::compute(const std::vector<Vec3>& points,
                                const TetDelaunayOptions& opts) noexcept
{
    TetMesh result;
    if (points.size() < 4) return result;

    result.vertices = points;

    // Compute bounding box.
    Vec3 bmin = points[0], bmax = points[0];
    for (const auto& p : points) {
        bmin.x = std::min(bmin.x, p.x);
        bmin.y = std::min(bmin.y, p.y);
        bmin.z = std::min(bmin.z, p.z);
        bmax.x = std::max(bmax.x, p.x);
        bmax.y = std::max(bmax.y, p.y);
        bmax.z = std::max(bmax.z, p.z);
    }

    // Create super-tetrahedron vertices that enclose all points.
    float dx = (bmax.x - bmin.x) * 0.5f + 1.f;
    float dy = (bmax.y - bmin.y) * 0.5f + 1.f;
    float dz = (bmax.z - bmin.z) * 0.5f + 1.f;
    float cx = (bmin.x + bmax.x) * 0.5f;
    float cy = (bmin.y + bmax.y) * 0.5f;
    float cz = (bmin.z + bmax.z) * 0.5f;

    float scale = std::max({dx, dy, dz}) * 6.f;
    uint32_t sv0 = static_cast<uint32_t>(result.vertices.size());
    result.vertices.push_back({cx,           cy,           cz - scale});
    result.vertices.push_back({cx - scale,   cy + scale,   cz + scale});
    result.vertices.push_back({cx + scale,   cy + scale,   cz + scale});
    result.vertices.push_back({cx,           cy - scale,   cz + scale});
    const uint32_t sv1 = sv0 + 1;
    const uint32_t sv2 = sv0 + 2;
    const uint32_t sv3 = sv0 + 3;
    const uint32_t nPoints = static_cast<uint32_t>(points.size());

    // Start with the super-tetrahedron.
    std::vector<Tet> tets;
    Tet super;
    super.v = std::array<uint32_t, 4>{sv0, sv1, sv2, sv3};
    if (computeCircumsphere(super, result.vertices)) {
        tets.push_back(super);
    }

    // Incremental Bowyer-Watson insertion.
    for (uint32_t pi = 0; pi < nPoints; ++pi) {
        const Vec3& p = points[pi];

        // Find all tetrahedra whose circumsphere contains p (cavity).
        std::vector<size_t> cavity;
        for (size_t ti = 0; ti < tets.size(); ++ti) {
            if (pointInCircumsphere(p, tets[ti], opts.epsilon)) {
                cavity.push_back(ti);
            }
        }

        if (cavity.empty()) continue;

        // Collect boundary triangles using FaceKey map.
        std::unordered_map<FaceKey, uint32_t, FaceKeyHash> faceCount;
        for (size_t idx : cavity) {
            const auto& v = tets[idx].v;
            std::array<std::array<uint32_t,3>, 4> faces = {{
                {{v[0], v[1], v[2]}}, {{v[0], v[1], v[3]}},
                {{v[0], v[2], v[3]}}, {{v[1], v[2], v[3]}}
            }};
            for (const auto& f : faces) {
                std::array<uint32_t,3> sf = f;
                if (sf[0] > sf[1]) std::swap(sf[0], sf[1]);
                if (sf[1] > sf[2]) std::swap(sf[1], sf[2]);
                if (sf[0] > sf[1]) std::swap(sf[0], sf[1]);
                faceCount[FaceKey{sf}]++;
            }
        }

        // Create new tetrahedra connecting p to each boundary face.
        for (const auto& [key, count] : faceCount) {
            if (count != 1) continue;
            Tet newTet;
            newTet.v = {pi, key.v[0], key.v[1], key.v[2]};
            if (computeCircumsphere(newTet, result.vertices)) {
                tets.push_back(newTet);
            }
        }

        // Remove cavity tetrahedra (in reverse order to preserve indices).
        std::sort(cavity.begin(), cavity.end(), std::greater<size_t>());
        for (size_t idx : cavity) {
            tets[idx] = tets.back();
            tets.pop_back();
        }
    }

    // Remove tetrahedra that use super-tetrahedron vertices.
    std::vector<Tet> filtered;
    for (const auto& tet : tets) {
        bool usesSuper = false;
        for (int i = 0; i < 4; ++i) {
            if (tet.v[i] >= nPoints) { usesSuper = true; break; }
        }
        if (!usesSuper) filtered.push_back(tet);
    }

    // Also remove super-tetrahedron vertices.
    result.vertices.resize(nPoints);

    // Copy filtered tetrahedra to result.
    result.tetrahedra.reserve(filtered.size());
    for (const auto& tet : filtered) {
        result.tetrahedra.push_back(tet.v);
    }

    return result;
}

} // namespace nexus::geometry
