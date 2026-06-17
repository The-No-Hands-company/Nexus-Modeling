#include <nexus/geometry/MeshSimplify.h>
#include <nexus/geometry/MeshDecimator.h>
#include <nexus/geometry/HalfEdgeMesh.h>

#include <algorithm>

namespace nexus::geometry {

Mesh MeshSimplify::decimate(const Mesh& mesh, const SimplifyOptions& opts) {
    if (!mesh.isValid()) return mesh;

    const size_t currentFaces = mesh.topology().faceCount();
    if (currentFaces == 0) return mesh;
    if (currentFaces <= opts.targetFaceCount) return mesh;

    // The quadric-error decimator collapses one edge at a time in a
    // half-edge structure. Near-complete reductions (>90% of faces
    // removed) risk producing non-manifold topology that the decimator
    // cannot recover from. The per-call limit ensures callers decimate
    // iteratively when large reductions are needed.
    if (opts.targetFaceCount < currentFaces * 9 / 10) return mesh;

    auto hemOpt = HalfEdgeMesh::fromMesh(mesh);
    if (!hemOpt) return mesh;

    DecimationOptions decOpts;
    decOpts.targetFaceCount = opts.targetFaceCount;

    auto result = MeshDecimator::decimate(*hemOpt, decOpts);
    if (!result) return mesh;

    return result->first.toMesh();
}

} // namespace nexus::geometry
