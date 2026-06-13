#include <nexus/geometry/MeshTangentSpace.h>

#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh MeshTangentSpace::compute(const Mesh& mesh) {
    Mesh result = mesh;
    if (!result.isValid()) return result;

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    const auto& norms = mesh.attributes().normals();
    const auto& uvs = mesh.attributes().uvs();

    if (!mesh.attributes().hasNormals() || !mesh.attributes().hasUVs())
        return result;

    size_t V = pos.size();
    std::vector<Vec3> tangents(V, {0, 0, 0});
    std::vector<Vec3> bitangents(V, {0, 0, 0});

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() < 3) continue;

        for (size_t vi = 0; vi + 2 < f.indices.size(); ++vi) {
            uint32_t i0 = f.indices[0];
            uint32_t i1 = f.indices[vi + 1];
            uint32_t i2 = f.indices[vi + 2];

            Vec3 e1 = pos[i1] - pos[i0];
            Vec3 e2 = pos[i2] - pos[i0];

            float du1 = uvs[i1].u - uvs[i0].u;
            float dv1 = uvs[i1].v - uvs[i0].v;
            float du2 = uvs[i2].u - uvs[i0].u;
            float dv2 = uvs[i2].v - uvs[i0].v;

            float denom = du1 * dv2 - du2 * dv1;
            float r = std::fabs(denom) > 1e-20f ? 1.f / denom : 0.f;

            Vec3 T = (e1 * dv2 - e2 * dv1) * r;
            Vec3 B = (e2 * du1 - e1 * du2) * r;

            tangents[i0] = tangents[i0] + T;
            tangents[i1] = tangents[i1] + T;
            tangents[i2] = tangents[i2] + T;
            bitangents[i0] = bitangents[i0] + B;
            bitangents[i1] = bitangents[i1] + B;
            bitangents[i2] = bitangents[i2] + B;
        }
    }

    std::vector<Vec4> resultTangents;
    resultTangents.reserve(V);

    for (size_t i = 0; i < V; ++i) {
        Vec3 N = norms[i];
        Vec3 T = tangents[i];

        float dotNT = T.dot(N);
        T = T - N * dotNT;
        float lenT = T.length();
        if (lenT > 1e-10f) T = T * (1.f / lenT);
        else T = {1.f, 0.f, 0.f};

        Vec3 B = bitangents[i];
        float handedness = N.cross(T).dot(B) >= 0.f ? 1.f : -1.f;

        resultTangents.push_back({T.x, T.y, T.z, handedness});
    }

    result.attributes().setTangents(std::move(resultTangents));
    return result;
}

} // namespace nexus::geometry
