#include <nexus/geometry/MeshPointSample.h>
#include "SplitMix64.h"

#include <algorithm>
#include <cmath>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

PointSampleResult MeshPointSample::sample(const Mesh& mesh, const PointSampleOptions& opts) {
    PointSampleResult result;
    if (!mesh.isValid() || opts.count == 0) return result;

    result.points.reserve(opts.count);
    result.normals.reserve(opts.count);
    result.triangles.reserve(opts.count);

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    const bool hasNorm = mesh.attributes().hasNormals();
    const auto& norms = hasNorm ? mesh.attributes().normals() : std::vector<Vec3>{};

    struct TriArea {
        uint32_t faceIdx;
        uint32_t vi0;
        uint32_t vi1;
        uint32_t vi2;
        float area;
        float cumulative;
    };
    std::vector<TriArea> areas;
    float totalArea = 0.f;

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& face = topo.face(fi);
        if (face.indices.size() < 3) continue;

        const Vec3& a = pos[face.indices[0]];
        for (size_t vi = 1; vi + 1 < face.indices.size(); ++vi) {
            const Vec3& b = pos[face.indices[vi]];
            const Vec3& c = pos[face.indices[vi + 1]];

            Vec3 e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
            Vec3 e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
            float area = e1.cross(e2).length() * 0.5f;

            totalArea += area;
            areas.push_back({static_cast<uint32_t>(fi),
                             face.indices[0], face.indices[vi], face.indices[vi + 1],
                             area, totalArea});
        }
    }

    if (areas.empty() || totalArea <= 0.f) return result;

    internal::SplitMix64 rng(opts.seed);

    for (uint32_t i = 0; i < opts.count; ++i) {
        float r = static_cast<float>(rng.uniform01()) * totalArea;

        auto it = std::lower_bound(areas.begin(), areas.end(), r,
            [](const TriArea& ta, float val) { return ta.cumulative < val; });

        if (it == areas.end()) it = areas.begin();
        const TriArea& ta = *it;

        float br1 = static_cast<float>(rng.uniform01());
        float br2 = static_cast<float>(rng.uniform01());
        if (br1 + br2 > 1.f) {
            br1 = 1.f - br1;
            br2 = 1.f - br2;
        }

        const Vec3& v0 = pos[ta.vi0];
        const Vec3& v1 = pos[ta.vi1];
        const Vec3& v2 = pos[ta.vi2];

        Vec3 pt = {
            v0.x + br1 * (v1.x - v0.x) + br2 * (v2.x - v0.x),
            v0.y + br1 * (v1.y - v0.y) + br2 * (v2.y - v0.y),
            v0.z + br1 * (v1.z - v0.z) + br2 * (v2.z - v0.z),
        };

        result.points.push_back(pt);

        if (hasNorm) {
            Vec3 nr = {
                norms[ta.vi0].x + br1 * (norms[ta.vi1].x - norms[ta.vi0].x)
                    + br2 * (norms[ta.vi2].x - norms[ta.vi0].x),
                norms[ta.vi0].y + br1 * (norms[ta.vi1].y - norms[ta.vi0].y)
                    + br2 * (norms[ta.vi2].y - norms[ta.vi0].y),
                norms[ta.vi0].z + br1 * (norms[ta.vi1].z - norms[ta.vi0].z)
                    + br2 * (norms[ta.vi2].z - norms[ta.vi0].z),
            };
            result.normals.push_back(nr);
        } else {
            result.normals.push_back({0.f, 0.f, 1.f});
        }

        result.triangles.push_back(ta.faceIdx);
    }

    return result;
}

} // namespace nexus::geometry
