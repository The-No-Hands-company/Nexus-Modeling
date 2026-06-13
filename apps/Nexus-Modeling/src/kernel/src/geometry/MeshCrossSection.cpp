#include <nexus/geometry/MeshCrossSection.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

std::vector<MeshCrossSection::Contour> MeshCrossSection::slice(
    const Mesh& mesh,
    const Vec3& origin,
    const Vec3& normal) {

    std::vector<Contour> contours;
    if (!mesh.isValid()) return contours;

    Vec3 n = normal;
    float nLen = n.length();
    if (nLen < 1e-10f) return contours;
    n = n * (1.0f / nLen);

    const auto& pos = mesh.attributes().positions();

    auto signedDist = [&](const Vec3& p) -> float {
        Vec3 diff = p - origin;
        return diff.dot(n);
    };

    struct EdgeIntersection {
        uint32_t v0, v1;
        Vec3 pt;
    };

    std::vector<EdgeIntersection> intersections;

    for (size_t fi = 0; fi < mesh.topology().faceCount(); ++fi) {
        const auto& face = mesh.topology().face(fi);
        size_t nv = face.indices.size();

        for (size_t ei = 0; ei < nv; ++ei) {
            size_t ej = (ei + 1) % nv;
            uint32_t i0 = face.indices[ei];
            uint32_t i1 = face.indices[ej];
            if (i0 >= pos.size() || i1 >= pos.size()) continue;

            float d0 = signedDist(pos[i0]);
            float d1 = signedDist(pos[i1]);

            if (d0 * d1 > 0.f) continue;

            float t = 0.f;
            if (std::abs(d1 - d0) > 1e-10f) {
                t = -d0 / (d1 - d0);
            }
            t = std::clamp(t, 0.f, 1.f);

            Vec3 pt{
                pos[i0].x + (pos[i1].x - pos[i0].x) * t,
                pos[i0].y + (pos[i1].y - pos[i0].y) * t,
                pos[i0].z + (pos[i1].z - pos[i0].z) * t,
            };

            intersections.push_back({i0, i1, pt});
        }
    }

    if (intersections.empty()) return contours;

    struct EndPointHash {
        size_t operator()(const Vec3& v) const noexcept {
            auto h = [](float f) -> uint64_t {
                return static_cast<uint64_t>(static_cast<int64_t>(f * 1000.f + 0.5f));
            };
            return h(v.x) ^ (h(v.y) << 21) ^ (h(v.z) << 42);
        }
    };

    struct EndPointEqual {
        bool operator()(const Vec3& a, const Vec3& b) const noexcept {
            return std::fabs(a.x - b.x) < 0.001f &&
                   std::fabs(a.y - b.y) < 0.001f &&
                   std::fabs(a.z - b.z) < 0.001f;
        }
    };

    std::unordered_map<uint32_t, std::vector<size_t>> adjacency;
    std::unordered_map<Vec3, std::vector<size_t>, EndPointHash, EndPointEqual> endpointMap;

    for (size_t i = 0; i < intersections.size(); ++i) {
        const auto& isect = intersections[i];
        endpointMap[isect.pt].push_back(i);
    }

    for (auto& [pt, indices] : endpointMap) {
        if (indices.size() == 2) {
            size_t a = indices[0];
            size_t b = indices[1];
            adjacency[static_cast<uint32_t>(a)].push_back(b);
            adjacency[static_cast<uint32_t>(b)].push_back(a);
        }
    }

    std::vector<bool> visited(intersections.size(), false);

    for (size_t i = 0; i < intersections.size(); ++i) {
        if (visited[i]) continue;

        Contour contour;
        size_t cur = i;
        visited[cur] = true;
        contour.points.push_back(intersections[cur].pt);

        bool grew = true;
        while (grew) {
            grew = false;
            uint32_t ck = static_cast<uint32_t>(cur);
            auto it = adjacency.find(ck);
            if (it == adjacency.end()) break;

            for (size_t next : it->second) {
                if (!visited[next]) {
                    contour.points.push_back(intersections[next].pt);
                    visited[next] = true;
                    cur = next;
                    grew = true;
                    break;
                }
            }
        }

        if (contour.points.size() >= 2) {
            float d = (contour.points.front() - contour.points.back()).length();
            contour.closed = d < 0.001f && contour.points.size() > 2;
            contours.push_back(std::move(contour));
        }
    }

    return contours;
}

} // namespace nexus::geometry
