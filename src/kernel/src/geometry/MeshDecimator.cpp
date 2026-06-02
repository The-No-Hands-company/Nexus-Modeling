#include <nexus/geometry/MeshDecimator.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <vector>

namespace nexus::geometry {

// ─── Quadric (4×4 symmetric matrix stored as upper-triangle 10 floats) ───────
// Plane equation: ax + by + cz + d = 0  →  Q = [a,b,c,d]^T [a,b,c,d]

struct Quadric {
    // Stored as: q00 q01 q02 q03 q11 q12 q13 q22 q23 q33
    double e[10] = {};

    static Quadric fromPlane(float nx, float ny, float nz, float d) noexcept
    {
        Quadric q;
        const double a = nx, b = ny, c = nz, dd = d;
        q.e[0] = a*a; q.e[1] = a*b; q.e[2] = a*c; q.e[3] = a*dd;
        q.e[4] = b*b; q.e[5] = b*c; q.e[6] = b*dd;
        q.e[7] = c*c; q.e[8] = c*dd;
        q.e[9] = dd*dd;
        return q;
    }

    Quadric& operator+=(const Quadric& o) noexcept
    {
        for (int i = 0; i < 10; ++i) e[i] += o.e[i];
        return *this;
    }

    Quadric operator+(const Quadric& o) const noexcept
    {
        Quadric r = *this;
        r += o;
        return r;
    }

    // Evaluate v^T Q v for homogeneous v = (x,y,z,1).
    double eval(float x, float y, float z) const noexcept
    {
        const double vx = x, vy = y, vz = z;
        return  e[0]*vx*vx + 2*e[1]*vx*vy + 2*e[2]*vx*vz + 2*e[3]*vx
               +e[4]*vy*vy + 2*e[5]*vy*vz + 2*e[6]*vy
               +e[7]*vz*vz + 2*e[8]*vz
               +e[9];
    }

    // Attempt to solve for the optimal position by inverting the 3×3 sub-system.
    // Returns false if the matrix is near-singular; caller should fall back to midpoint.
    bool optimalPosition(float& ox, float& oy, float& oz) const noexcept
    {
        // 3×3 system: [q00 q01 q02][x]   [-q03]
        //             [q01 q11 q12][y] = [-q13]
        //             [q02 q12 q22][z]   [-q23]
        const double a00 = e[0], a01 = e[1], a02 = e[2];
        const double a11 = e[4], a12 = e[5];
        const double a22 = e[7];
        const double det = a00*(a11*a22 - a12*a12)
                         - a01*(a01*a22 - a12*a02)
                         + a02*(a01*a12 - a11*a02);
        if (std::abs(det) < 1e-10) { return false; }
        const double inv = 1.0 / det;
        const double b0 = -e[3], b1 = -e[6], b2 = -e[8];
        ox = static_cast<float>(inv * (b0*(a11*a22 - a12*a12) - a01*(b1*a22 - a12*b2) + a02*(b1*a12 - a11*b2)));
        oy = static_cast<float>(inv * (a00*(b1*a22 - a12*b2) - b0*(a01*a22 - a12*a02) + a02*(a01*b2 - b1*a02)));
        oz = static_cast<float>(inv * (a00*(a11*b2 - b1*a12) - a01*(a01*b2 - b1*a02) + b0*(a01*a12 - a11*a02)));
        return true;
    }
};

// ─── decimate ─────────────────────────────────────────────────────────────────

std::optional<std::pair<HalfEdgeMesh, DecimationReport>>
MeshDecimator::decimate(const HalfEdgeMesh& input, const DecimationOptions& opts)
{
    if (!input.isManifold() || input.faceCount() == 0) { return std::nullopt; }

    // Work on a copy.
    auto meshOpt = HalfEdgeMesh::fromMesh(input.toMesh());
    if (!meshOpt) { return std::nullopt; }
    HalfEdgeMesh mesh = std::move(*meshOpt);

    DecimationReport report;
    report.facesIn = mesh.faceCount();

    const uint32_t target = (opts.targetFaceCount > 0)
        ? opts.targetFaceCount
        : static_cast<uint32_t>(std::ceil(report.facesIn * opts.targetRatio));

    if (target >= report.facesIn) {
        report.facesOut = report.facesIn;
        return std::make_pair(std::move(mesh), report);
    }

    const auto& initialVerts = mesh.vertices();
    const uint32_t nV = mesh.vertexCount();
    const uint32_t nHE = mesh.halfEdgeCount();

    // ── Step 1: compute per-vertex quadrics ──────────────────────────────────
    std::vector<Quadric> Q(nV);
    // For each face, compute its plane and accumulate into vertex quadrics.
    for (uint32_t fi = 0; fi < mesh.faceCount(); ++fi) {
        if (mesh.faces()[fi].halfEdge == kHEInvalid) { continue; }
        auto fv = mesh.faceVertices(fi);
        if (fv.size() < 3) { continue; }
        const auto& p0 = initialVerts[fv[0]].position;
        const auto& p1 = initialVerts[fv[1]].position;
        const auto& p2 = initialVerts[fv[2]].position;
        // Normal via cross product.
        float nx = (p1.y-p0.y)*(p2.z-p0.z) - (p1.z-p0.z)*(p2.y-p0.y);
        float ny = (p1.z-p0.z)*(p2.x-p0.x) - (p1.x-p0.x)*(p2.z-p0.z);
        float nz = (p1.x-p0.x)*(p2.y-p0.y) - (p1.y-p0.y)*(p2.x-p0.x);
        const float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (len < 1e-12f) { continue; }
        nx /= len; ny /= len; nz /= len;
        const float d = -(nx*p0.x + ny*p0.y + nz*p0.z);
        const Quadric qf = Quadric::fromPlane(nx, ny, nz, d);
        for (uint32_t vi : fv) { Q[vi] += qf; }
    }

    // ── Step 2: build priority queue of edge collapses ───────────────────────
    struct CollapseCandidate {
        float    error;
        uint32_t heIdx;
        float    px, py, pz; // optimal collapse position
        bool operator>(const CollapseCandidate& o) const noexcept { return error > o.error; }
    };

    auto buildCandidate = [&](uint32_t hi) -> std::optional<CollapseCandidate> {
        const auto& hes = mesh.halfEdges();
        const auto& vs  = mesh.vertices();
        const auto& he = hes[hi];
        if (he.src == kHEInvalid || he.dst == kHEInvalid) { return std::nullopt; }
        if (he.face == kHEInvalid) { return std::nullopt; }
        if (opts.preserveBoundary && he.twin == kHEInvalid) { return std::nullopt; }
        if (he.src >= Q.size() || he.dst >= Q.size()) { return std::nullopt; }
        const Quadric qe = Q[he.src] + Q[he.dst];
        float px, py, pz;
        if (!qe.optimalPosition(px, py, pz)) {
            const auto& ps = vs[he.src].position;
            const auto& pd = vs[he.dst].position;
            px = (ps.x + pd.x) * 0.5f;
            py = (ps.y + pd.y) * 0.5f;
            pz = (ps.z + pd.z) * 0.5f;
        }
        const float err = static_cast<float>(qe.eval(px, py, pz));
        if (err > opts.maxError) { return std::nullopt; }
        return CollapseCandidate{err, hi, px, py, pz};
    };

    std::priority_queue<CollapseCandidate,
                        std::vector<CollapseCandidate>,
                        std::greater<CollapseCandidate>> pq;

    for (uint32_t hi = 0; hi < nHE; ++hi) {
        if (auto c = buildCandidate(hi)) {
            pq.push(*c);
        }
    }

    // ── Step 3: collapse loop ─────────────────────────────────────────────────
    uint32_t liveFaces = mesh.faceCount();
    // Count live (non-invalid) faces.
    liveFaces = 0;
    for (uint32_t fi = 0; fi < mesh.faceCount(); ++fi) {
        if (mesh.faces()[fi].halfEdge != kHEInvalid) { ++liveFaces; }
    }

    while (liveFaces > target && !pq.empty()) {
        const auto cand = pq.top();
        pq.pop();

        // Validate the candidate is still alive.
        const auto& hes = mesh.halfEdges();
        if (cand.heIdx >= hes.size()) { continue; }
        const auto& he = hes[cand.heIdx];
        if (he.src == kHEInvalid || he.dst == kHEInvalid) { continue; }
        if (he.face == kHEInvalid) { continue; }
        if (opts.preserveBoundary && he.twin == kHEInvalid) { continue; }

        if (he.src >= Q.size() || he.dst >= Q.size()) { continue; }

        const uint32_t vSrc = he.src;
        const uint32_t vDst = he.dst;

        // Attempt the collapse.
        const uint32_t survivor = mesh.collapseEdge(cand.heIdx);
        if (survivor == kHEInvalid) { continue; }

        // Update survivor position and quadric.
        // The HalfEdgeMesh collapseEdge places the survivor at the midpoint;
        // we want the optimal position, so we accept whichever vertex survived.
        // Q[survivor] = Q[vSrc] + Q[vDst].
        if (survivor < Q.size()) {
            Q[survivor] = Q[vSrc] + Q[vDst];
        }

        report.collapses++;
        report.maxErrorApplied = std::max(report.maxErrorApplied, cand.error);

        // Recount live faces (collapseEdge removes 1 or 2 faces).
        liveFaces = 0;
        for (uint32_t fi = 0; fi < mesh.faceCount(); ++fi) {
            if (mesh.faces()[fi].halfEdge != kHEInvalid) { ++liveFaces; }
        }

        // Re-queue edges incident to the survivor vertex.
        for (uint32_t hi = 0; hi < static_cast<uint32_t>(mesh.halfEdges().size()); ++hi) {
            const auto& h = mesh.halfEdges()[hi];
            if (h.src == survivor || h.dst == survivor) {
                if (auto c = buildCandidate(hi)) { pq.push(*c); }
            }
        }
    }

    report.facesOut = liveFaces;

    // Compact the mesh (build a new clean mesh from the live faces/vertices).
    auto compacted = HalfEdgeMesh::fromMesh(mesh.toMesh());
    if (!compacted) { return std::nullopt; }

    return std::make_pair(std::move(*compacted), report);
}

} // namespace nexus::geometry
