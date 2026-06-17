#include <nexus/geometry/MeshNormals.h>

#include <cmath>
#include <tuple>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

bool MeshNormals::computeSmooth(Mesh& mesh) {
    NormalOptions opts;
    opts.mode = NormalMode::Smooth;
    return compute(mesh, opts);
}

bool MeshNormals::compute(Mesh& mesh, const NormalOptions& opts) {
    if (!mesh.isValid()) return false;
    if (!mesh.attributes().hasPositions()) return false;

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();

    if (opts.mode == NormalMode::Flat) {
        std::ignore = mesh.topology().triangulate();

        std::vector<Vec3> origPos = pos;
        std::vector<Vec3> flatPos;
        std::vector<Face> flatFaces;
        flatPos.reserve(topo.faceCount() * 3);
        flatFaces.reserve(topo.faceCount());

        uint32_t idx = 0;
        for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
            const Face& face = topo.face(fi);
            if (!face.indicesInBounds(pos.size())) continue;
            if (face.indices.size() < 3) continue;

            const Vec3& a = origPos[face.indices[0]];
            const Vec3& b = origPos[face.indices[1]];
            const Vec3& c = origPos[face.indices[2]];

            flatPos.push_back(a);
            flatPos.push_back(b);
            flatPos.push_back(c);
            flatFaces.push_back(Face{{idx, idx + 1, idx + 2}});
            idx += 3;
        }

        mesh.topology().clearFaces();
        for (const auto& f : flatFaces) {
            mesh.topology().addFace(f);
        }
        mesh.attributes().setPositions(std::move(flatPos));
        if (!mesh.computeVertexNormals()) return false;
        mesh.rebuildStableElementIds();
        return true;
    }

    if (opts.mode == NormalMode::CreaseAngle) {
        float creaseCos = std::cos(opts.creaseAngleDeg * 3.14159265358979323846f / 180.f);

        std::vector<Vec3> faceNorms;
        faceNorms.reserve(topo.faceCount());
        for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
            const Face& face = topo.face(fi);
            if (!face.indicesInBounds(pos.size())) {
                faceNorms.push_back({0.f, 0.f, 1.f});
                continue;
            }
            if (face.indices.size() < 3) {
                faceNorms.push_back({0.f, 0.f, 1.f});
                continue;
            }
            const Vec3& a = pos[face.indices[0]];
            const Vec3& b = pos[face.indices[1]];
            const Vec3& c = pos[face.indices[2]];
            Vec3 e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
            Vec3 e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
            faceNorms.push_back(e1.cross(e2).normalize());
        }

        std::vector<Vec3> vnorms(pos.size(), Vec3{0.f, 0.f, 0.f});
        std::vector<std::vector<std::pair<uint32_t, Vec3>>> vertexFaces(pos.size());

        for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
            const Face& face = topo.face(fi);
            if (!face.indicesInBounds(pos.size())) continue;
            if (face.indices.size() < 3) continue;
            for (size_t vi = 1; vi + 1 < face.indices.size(); ++vi) {
                const Vec3& a = pos[face.indices[0]];
                const Vec3& b = pos[face.indices[vi]];
                const Vec3& c = pos[face.indices[vi + 1]];
                Vec3 e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
                Vec3 e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
                Vec3 fn = e1.cross(e2);
                float area = fn.length() * 0.5f;
                Vec3 nf = fn.normalize();

                uint32_t idx0 = face.indices[0];
                uint32_t idx1 = face.indices[vi];
                uint32_t idx2 = face.indices[vi + 1];
                vertexFaces[idx0].push_back({static_cast<uint32_t>(fi),
                    Vec3{nf.x * area, nf.y * area, nf.z * area}});
                vertexFaces[idx1].push_back({static_cast<uint32_t>(fi),
                    Vec3{nf.x * area, nf.y * area, nf.z * area}});
                vertexFaces[idx2].push_back({static_cast<uint32_t>(fi),
                    Vec3{nf.x * area, nf.y * area, nf.z * area}});
            }
        }

        for (size_t vi = 0; vi < pos.size(); ++vi) {
            auto& vfs = vertexFaces[vi];
            if (vfs.empty()) {
                vnorms[vi] = {0.f, 0.f, 1.f};
                continue;
            }

            if (vfs.size() == 1) {
                vnorms[vi] = faceNorms[vfs[0].first];
                continue;
            }

            Vec3& dest = vnorms[vi];

            for (size_t i = 0; i < vfs.size(); ++i) {
                Vec3 fni = faceNorms[vfs[i].first];
                bool creaseBreak = false;
                for (size_t j = 0; j < vfs.size(); ++j) {
                    if (i == j) continue;
                    Vec3 fnj = faceNorms[vfs[j].first];
                    if (fni.dot(fnj) < creaseCos) {
                        creaseBreak = true;
                        break;
                    }
                }

                if (!creaseBreak) {
                    dest.x += vfs[i].second.x;
                    dest.y += vfs[i].second.y;
                    dest.z += vfs[i].second.z;
                } else {
                    dest.x += vfs[i].second.x * 0.1f;
                    dest.y += vfs[i].second.y * 0.1f;
                    dest.z += vfs[i].second.z * 0.1f;
                }
            }
        }

        for (auto& n : vnorms) {
            n = n.normalize();
        }

        mesh.attributes().setNormals(std::move(vnorms));
        mesh.rebuildStableElementIds();
        return true;
    }

    if (!mesh.computeVertexNormals()) return false;
    mesh.rebuildStableElementIds();
    return true;
}

} // namespace nexus::geometry
