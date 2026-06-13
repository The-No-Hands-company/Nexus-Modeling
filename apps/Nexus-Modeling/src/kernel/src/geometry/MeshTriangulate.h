#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry::internal {

using Vec3 = nexus::render::Vec3;

struct FanTriangle {
    Vec3     a, b, c;
    uint32_t srcFace = 0u;
};

inline std::vector<FanTriangle> triangulateFaces(const nexus::geometry::Mesh& mesh) noexcept {
    const auto& topology  = mesh.topology();
    const auto& positions = mesh.attributes().positions();
    const size_t faceCount = topology.faceCount();

    std::vector<FanTriangle> result;

    for (size_t fi = 0; fi < faceCount; ++fi) {
        const auto& face = topology.face(fi);
        const size_t nv = face.vertexCount();
        if (nv < 3) continue;

        const Vec3& v0 = positions[face.indices[0]];

        for (size_t vi = 1; vi + 1 < nv; ++vi) {
            const Vec3& v1 = positions[face.indices[vi]];
            const Vec3& v2 = positions[face.indices[vi + 1]];

            result.push_back(FanTriangle{v0, v1, v2, static_cast<uint32_t>(fi)});
        }
    }

    return result;
}

} // namespace nexus::geometry::internal
