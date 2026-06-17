#include <nexus/geometry/SurfaceOffset.h>

#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh SurfaceOffset::offset(const Mesh& mesh, const SurfaceOffsetOptions& opts) {
    Mesh result = mesh;
    if (!result.isValid()) return result;

    const auto& topo = result.topology();
    const auto& pos = result.attributes().positions();

    std::vector<Vec3> vnorms(pos.size(), Vec3{0.f, 0.f, 0.f});

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& face = topo.face(fi);
        if (!face.indicesInBounds(pos.size())) continue;
        if (face.indices.size() < 3) continue;

        for (size_t vi = 0; vi + 2 < face.indices.size(); ++vi) {
            const Vec3& a = pos[face.indices[0]];
            const Vec3& b = pos[face.indices[vi + 1]];
            const Vec3& c = pos[face.indices[vi + 2]];

            Vec3 e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
            Vec3 e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
            Vec3 fn = e1.cross(e2);

            for (size_t j = 0; j < 3; ++j) {
                uint32_t idx = face.indices[vi + j];
                vnorms[idx].x += fn.x;
                vnorms[idx].y += fn.y;
                vnorms[idx].z += fn.z;
            }
        }
    }

    for (auto& n : vnorms) {
        n = n.normalize();
    }

    std::vector<Vec3> newPos = pos;
    for (size_t i = 0; i < newPos.size(); ++i) {
        newPos[i].x += vnorms[i].x * opts.distance;
        newPos[i].y += vnorms[i].y * opts.distance;
        newPos[i].z += vnorms[i].z * opts.distance;
    }

    result.attributes().setPositions(std::move(newPos));

    if (opts.recomputeNormals) {
        if (!result.computeVertexNormals()) return result;
    }

    result.rebuildStableElementIds();
    return result;
}

} // namespace nexus::geometry
