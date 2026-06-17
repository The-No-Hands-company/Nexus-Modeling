#include <nexus/geometry/FeatureLineExtractor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

static constexpr float kPi = 3.14159265358979323846f;

std::vector<FeatureLineExtractor::Polyline> FeatureLineExtractor::extract(
    const Mesh& surface, const FeatureLineExtractorOptions& opts) {

    std::vector<Polyline> polylines;
    if (!surface.isValid()) return polylines;

    const auto& topo = surface.topology();
    const auto& pos  = surface.attributes().positions();
    size_t faceCount  = topo.faceCount();
    if (faceCount < 2 || pos.empty()) return polylines;

    float thresholdRad = opts.dihedralAngleThresholdDegrees * kPi / 180.0f;

    std::unordered_map<uint64_t, std::vector<size_t>> edgeToFaces;

    for (size_t fi = 0; fi < faceCount; ++fi) {
        const Face& face = topo.face(fi);
        const auto& idx  = face.indices;
        size_t vc = idx.size();
        if (vc < 3) continue;
        for (size_t vi = 0; vi < vc; ++vi) {
            uint32_t a = idx[vi];
            uint32_t b = idx[(vi + 1) % vc];
            uint64_t key = (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
            edgeToFaces[key].push_back(fi);
        }
    }

    std::vector<Vec3> faceNormals(faceCount, Vec3{0.0f, 0.0f, 0.0f});
    for (size_t fi = 0; fi < faceCount; ++fi) {
        const Face& face = topo.face(fi);
        if (!face.indicesInBounds(pos.size())) continue;
        if (face.indices.size() < 3) continue;
        const Vec3& a = pos[face.indices[0]];
        const Vec3& b = pos[face.indices[1]];
        const Vec3& c = pos[face.indices[2]];
        Vec3 n = (b - a).cross(c - a);
        float len = n.length();
        if (len > 1e-10f) {
            faceNormals[fi] = n * (1.0f / len);
        }
    }

    struct FeatureEdge {
        Vec3     a;
        Vec3     b;
        uint32_t va;
        uint32_t vb;
    };

    std::vector<FeatureEdge> featureEdges;

    for (const auto& [key, faces] : edgeToFaces) {
        if (faces.size() < 2) continue;

        uint32_t va = static_cast<uint32_t>(key >> 32);
        uint32_t vb = static_cast<uint32_t>(key & 0xFFFFFFFFULL);

        Vec3 n1 = faceNormals[faces[0]];
        Vec3 n2 = faceNormals[faces[1]];

        float dot = std::max(-1.0f, std::min(1.0f, n1.dot(n2)));
        float angle = std::acos(dot);

        if (angle >= thresholdRad) {
            featureEdges.push_back({pos[va], pos[vb], va, vb});
        }
    }

    if (featureEdges.empty()) return polylines;

    std::unordered_map<uint32_t, std::vector<size_t>> vertexToEdges;
    for (size_t ei = 0; ei < featureEdges.size(); ++ei) {
        vertexToEdges[featureEdges[ei].va].push_back(ei);
        vertexToEdges[featureEdges[ei].vb].push_back(ei);
    }

    std::vector<bool> used(featureEdges.size(), false);

    for (size_t ei = 0; ei < featureEdges.size(); ++ei) {
        if (used[ei]) continue;

        Polyline poly;
        used[ei] = true;
        poly.points.push_back(featureEdges[ei].a);
        poly.points.push_back(featureEdges[ei].b);

        uint32_t frontVertex = featureEdges[ei].va;
        uint32_t backVertex  = featureEdges[ei].vb;

        bool grown = true;
        while (grown) {
            grown = false;
            auto it = vertexToEdges.find(backVertex);
            if (it != vertexToEdges.end()) {
                for (size_t neighborIdx : it->second) {
                    if (used[neighborIdx]) continue;
                    const auto& fe = featureEdges[neighborIdx];
                    if (fe.va == backVertex) {
                        poly.points.push_back(fe.b);
                        backVertex = fe.vb;
                        used[neighborIdx] = true;
                        grown = true;
                        break;
                    }
                    if (fe.vb == backVertex) {
                        poly.points.push_back(fe.a);
                        backVertex = fe.va;
                        used[neighborIdx] = true;
                        grown = true;
                        break;
                    }
                }
            }
        }

        grown = true;
        while (grown) {
            grown = false;
            auto it = vertexToEdges.find(frontVertex);
            if (it != vertexToEdges.end()) {
                for (size_t neighborIdx : it->second) {
                    if (used[neighborIdx]) continue;
                    const auto& fe = featureEdges[neighborIdx];
                    if (fe.vb == frontVertex) {
                        poly.points.insert(poly.points.begin(), fe.a);
                        frontVertex = fe.va;
                        used[neighborIdx] = true;
                        grown = true;
                        break;
                    }
                    if (fe.va == frontVertex) {
                        poly.points.insert(poly.points.begin(), fe.b);
                        frontVertex = fe.vb;
                        used[neighborIdx] = true;
                        grown = true;
                        break;
                    }
                }
            }
        }

        poly.closed = (frontVertex == backVertex && poly.points.size() > 2);
        if (poly.closed) {
            poly.points.pop_back();
        }

        polylines.push_back(std::move(poly));
    }

    return polylines;
}

} // namespace nexus::geometry
