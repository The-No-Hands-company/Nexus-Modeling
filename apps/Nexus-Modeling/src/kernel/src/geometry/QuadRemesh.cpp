#include <nexus/geometry/QuadRemesh.h>
#include <nexus/geometry/MeshBVH.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh QuadRemesh::remesh(const Mesh& surface, const QuadRemeshOptions& opts) {
    Mesh result;

    if (!surface.isValid() || opts.resolutionU < 2 || opts.resolutionV < 2) return result;

    auto bounds = surface.computeBounds();
    if (bounds.min == bounds.max) return result;

    Vec3 extent = bounds.extents();
    Vec3 center = bounds.center();
    Vec3 halfDim = extent;

    int32_t resU = opts.resolutionU;
    int32_t resV = opts.resolutionV;

    MeshBVH bvh;
    bvh.build(surface);

    auto& attrs = result.attributes();
    auto& topo  = result.topology();

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    positions.reserve(static_cast<size_t>(resU * resV));
    normals.reserve(static_cast<size_t>(resU * resV));

    for (int32_t j = 0; j < resV; ++j) {
        float v = (static_cast<float>(j) / static_cast<float>(resV - 1) - 0.5f) * 2.0f;
        for (int32_t i = 0; i < resU; ++i) {
            float u = (static_cast<float>(i) / static_cast<float>(resU - 1) - 0.5f) * 2.0f;
            Vec3 gridPt = {center.x + u * halfDim.x,
                           center.y + v * halfDim.y,
                           center.z};

            auto hit = bvh.closestPoint(gridPt);
            if (hit.valid) {
                positions.push_back(hit.point);
                normals.push_back(bvh.tris()[hit.triangleIndex].normal());
            } else {
                positions.push_back(gridPt);
                normals.push_back({0.f, 0.f, 1.f});
            }
        }
    }

    for (int32_t j = 0; j < resV - 1; ++j) {
        for (int32_t i = 0; i < resU - 1; ++i) {
            uint32_t a = static_cast<uint32_t>(j * resU + i);
            uint32_t b = static_cast<uint32_t>(j * resU + i + 1);
            uint32_t c = static_cast<uint32_t>((j + 1) * resU + i + 1);
            uint32_t d = static_cast<uint32_t>((j + 1) * resU + i);
            Face f; f.indices = {a, b, c, d};
            topo.addFace(f);
        }
    }

    attrs.setPositions(positions);
    attrs.setNormals(normals);
    return result;
}

} // namespace nexus::geometry
