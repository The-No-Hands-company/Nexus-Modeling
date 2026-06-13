#include <nexus/geometry/MeshWindingNumber.h>

#include <algorithm>
#include <cmath>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

float MeshWindingNumber::at(const Mesh& mesh, const Vec3& query) {
    if (!mesh.isValid()) return 0.f;

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();

    double w = 0.0;

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& face = topo.face(fi);
        if (face.indices.size() < 3) continue;

        for (size_t vi = 0; vi + 2 < face.indices.size(); ++vi) {
            const Vec3& p0 = pos[face.indices[0]];
            const Vec3& p1 = pos[face.indices[vi + 1]];
            const Vec3& p2 = pos[face.indices[vi + 2]];

            double a0x = static_cast<double>(p0.x - query.x);
            double a0y = static_cast<double>(p0.y - query.y);
            double a0z = static_cast<double>(p0.z - query.z);

            double a1x = static_cast<double>(p1.x - query.x);
            double a1y = static_cast<double>(p1.y - query.y);
            double a1z = static_cast<double>(p1.z - query.z);

            double a2x = static_cast<double>(p2.x - query.x);
            double a2y = static_cast<double>(p2.y - query.y);
            double a2z = static_cast<double>(p2.z - query.z);

            double d0 = std::sqrt(a0x * a0x + a0y * a0y + a0z * a0z);
            double d1 = std::sqrt(a1x * a1x + a1y * a1y + a1z * a1z);
            double d2 = std::sqrt(a2x * a2x + a2y * a2y + a2z * a2z);

            if (d0 < 1e-20 || d1 < 1e-20 || d2 < 1e-20) continue;

            double det = a0x * (a1y * a2z - a2y * a1z)
                       + a0y * (a1z * a2x - a2z * a1x)
                       + a0z * (a1x * a2y - a2x * a1y);

            double denom = d0 * d1 * d2
                         + d0 * (a1x * a2x + a1y * a2y + a1z * a2z)
                         + d1 * (a0x * a2x + a0y * a2y + a0z * a2z)
                         + d2 * (a0x * a1x + a0y * a1y + a0z * a1z);

            w += 2.0 * std::atan2(det, denom);
        }
    }

    w /= (4.0 * 3.14159265358979323846);
    return static_cast<float>(w);
}

std::vector<float> MeshWindingNumber::batch(const Mesh& mesh, const std::vector<Vec3>& queries) {
    std::vector<float> result;
    result.reserve(queries.size());
    for (const auto& q : queries) {
        result.push_back(at(mesh, q));
    }
    return result;
}

bool MeshWindingNumber::isInside(const Mesh& mesh, const Vec3& query, const WindingNumberOptions& opts) {
    float wn = at(mesh, query);
    return std::fabs(std::fabs(wn) - 1.f) < opts.epsilon || std::fabs(wn) > opts.isInsideThreshold;
}

} // namespace nexus::geometry
