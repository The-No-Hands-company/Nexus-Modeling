#include <nexus/geometry/MeshRepair.h>
#include <nexus/geometry/HalfEdgeMesh.h>

#include <map>
#include <utility>
#include <vector>

namespace nexus::geometry {

std::pair<Mesh, MeshRepairReport> MeshRepair::repair(const Mesh& inputMesh,
                                                      const MeshRepairOptions& opts)
{
    MeshRepairReport report;

    // ── Step 1: detect non-manifold edges ────────────────────────────────────
    // Build a map from undirected edge (minV, maxV) → face count.
    if (opts.resolveNonManifold) {
        using EKey = std::pair<uint32_t, uint32_t>;
        std::map<EKey, uint32_t> edgeFaceCount;
        const auto& topology = inputMesh.topology();
        for (uint32_t fi = 0; fi < static_cast<uint32_t>(topology.faceCount()); ++fi) {
            const auto& face = topology.face(fi);
            const uint32_t n = static_cast<uint32_t>(face.indices.size());
            for (uint32_t i = 0; i < n; ++i) {
                uint32_t a = face.indices[i];
                uint32_t b = face.indices[(i + 1) % n];
                if (a > b) { std::swap(a, b); }
                edgeFaceCount[{a, b}]++;
            }
        }
        for (const auto& [e, cnt] : edgeFaceCount) {
            if (cnt > 2) { report.nonManifoldEdgesFound++; }
        }
        // Resolution of non-manifold edges is complex and would require removing
        // duplicate faces; we report the count and leave the mesh unchanged for
        // that component (full resolution is an iterative separate pass).
        report.nonManifoldEdgesResolved = 0;
        if (report.nonManifoldEdgesFound > 0) {
            report.warnings.push_back(
                "Non-manifold edges detected (" +
                std::to_string(report.nonManifoldEdgesFound) +
                "); automated resolution not applied.");
        }
    }

    // ── Step 2: hole filling via HalfEdgeMesh boundary loops ─────────────────
    Mesh result = inputMesh; // start with a copy

    if (opts.fillHoles) {
        // Build half-edge mesh to find boundary loops.
        auto hemOpt = HalfEdgeMesh::fromMesh(result);
        if (!hemOpt) {
            report.warnings.push_back("HalfEdgeMesh construction failed; skipping hole fill.");
            return {result, report};
        }
        auto& hem = *hemOpt;
        report.inputWasClosed = hem.isClosed();

        const auto loops = hem.boundaryLoops();
        const auto& positions = result.attributes().positions();

        for (const auto& loop : loops) {
            if (loop.empty()) { continue; }
            if (opts.maxHoleEdges > 0 &&
                static_cast<uint32_t>(loop.size()) > opts.maxHoleEdges) {
                report.warnings.push_back(
                    "Hole with " + std::to_string(loop.size()) +
                    " edges skipped (exceeds maxHoleEdges).");
                continue;
            }

            report.holeBoundaryEdgeCount += static_cast<uint32_t>(loop.size());

            // Compute centroid of the boundary loop.
            nexus::render::Vec3 centroid{0, 0, 0};
            for (uint32_t vi : loop) {
                if (vi < positions.size()) {
                    centroid.x += positions[vi].x;
                    centroid.y += positions[vi].y;
                    centroid.z += positions[vi].z;
                }
            }
            const float inv = 1.f / loop.size();
            centroid.x *= inv;
            centroid.y *= inv;
            centroid.z *= inv;

            // Add the centroid as a new vertex.
            MeshAttributes newAttrs = result.attributes();
            auto pts = newAttrs.positions();
            const uint32_t centroidIdx = static_cast<uint32_t>(pts.size());
            pts.push_back(centroid);
            newAttrs.setPositions(std::move(pts));
            result.attributes() = std::move(newAttrs);

            // Fan triangles: (loop[i], loop[(i+1)%n], centroid)
            // The boundary loop from boundaryLoops() is ordered CCW when viewed
            // from outside, so we need CW winding to match the mesh convention
            // (we reverse by using centroid as v0).
            const uint32_t n = static_cast<uint32_t>(loop.size());
            for (uint32_t i = 0; i < n; ++i) {
                Face f;
                // Reverse to outward-normal: centroid, loop[i+1], loop[i]
                f.indices = {centroidIdx, loop[(i + 1) % n], loop[i]};
                result.topology().addFace(std::move(f));
            }

            report.holesFilled++;
        }

        // Re-check if closed after filling.
        auto hemAfterOpt = HalfEdgeMesh::fromMesh(result);
        if (hemAfterOpt) {
            report.outputIsClosed = hemAfterOpt->isClosed();
        }
    }

    return {result, report};
}

} // namespace nexus::geometry
