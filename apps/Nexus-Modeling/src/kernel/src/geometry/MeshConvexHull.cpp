#include <nexus/geometry/MeshConvexHull.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kCoplanarRelativeEpsilon = 1e-6f;

struct HullFace {
    uint32_t a, b, c;
    Vec3 normal;
    float offset;
    bool removed = false;
};

Vec3 computeNormal(Vec3 p0, Vec3 p1, Vec3 p2) {
    Vec3 e1 = p1 - p0;
    Vec3 e2 = p2 - p0;
    Vec3 c = e1.cross(e2);
    float lenSq = c.x * c.x + c.y * c.y + c.z * c.z;
    if (lenSq < 1e-24f) return {0.f, 0.f, 0.f};
    float inv = 1.f / std::sqrt(lenSq);
    return {c.x * inv, c.y * inv, c.z * inv};
}

float distanceToPlane(const Vec3& normal, float d, const Vec3& p) {
    return normal.dot(p) - d;
}

std::pair<uint64_t, uint64_t> makeEdge(uint32_t u, uint32_t v) {
    if (u < v) return {static_cast<uint64_t>(u), static_cast<uint64_t>(v)};
    return {static_cast<uint64_t>(v), static_cast<uint64_t>(u)};
}

// Use bit_cast to avoid -ffast-math silently optimising NaN checks away.
[[nodiscard]] bool isFiniteVec3(const Vec3& v) noexcept {
    const std::uint32_t bx = std::bit_cast<std::uint32_t>(v.x);
    const std::uint32_t by = std::bit_cast<std::uint32_t>(v.y);
    const std::uint32_t bz = std::bit_cast<std::uint32_t>(v.z);
    return (bx & 0x7F800000u) != 0x7F800000u &&
           (by & 0x7F800000u) != 0x7F800000u &&
           (bz & 0x7F800000u) != 0x7F800000u;
}

} // namespace

ConvexHull MeshConvexHull::build(const std::vector<Vec3>& points) {
    ConvexHull result;

    // Filter out NaN/Inf points first.
    std::vector<Vec3> cleanPoints;
    cleanPoints.reserve(points.size());
    for (const auto& p : points) {
        if (isFiniteVec3(p)) cleanPoints.push_back(p);
    }
    if (cleanPoints.size() < 4) return result;

    const size_t n = cleanPoints.size();
    const auto& pts = cleanPoints;

    size_t extremes[6] = {};
    float minVals[3] = {std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max()};
    float maxVals[3] = {std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest()};
    for (size_t i = 0; i < n; ++i) {
        const Vec3& p = pts[i];
        if (p.x < minVals[0]) { minVals[0] = p.x; extremes[0] = i; }
        if (p.x > maxVals[0]) { maxVals[0] = p.x; extremes[1] = i; }
        if (p.y < minVals[1]) { minVals[1] = p.y; extremes[2] = i; }
        if (p.y > maxVals[1]) { maxVals[1] = p.y; extremes[3] = i; }
        if (p.z < minVals[2]) { minVals[2] = p.z; extremes[4] = i; }
        if (p.z > maxVals[2]) { maxVals[2] = p.z; extremes[5] = i; }
    }

    // Compute mesh extents to scale the coplanarity/visibility epsilon.
    const float extX = maxVals[0] - minVals[0];
    const float extY = maxVals[1] - minVals[1];
    const float extZ = maxVals[2] - minVals[2];
    const float meshExtent = std::max({extX, extY, extZ});
    const float kVisibilityEpsilon = (meshExtent > 0.f)
        ? kCoplanarRelativeEpsilon * meshExtent
        : 1e-6f;

    std::vector<uint32_t> hullVerts;
    std::unordered_set<uint32_t> usedIdx;
    for (size_t i = 0; i < 6; ++i) {
        if (usedIdx.insert(static_cast<uint32_t>(extremes[i])).second) {
            hullVerts.push_back(static_cast<uint32_t>(extremes[i]));
        }
    }

    if (hullVerts.size() < 4) {
        for (size_t i = 0; i < n && hullVerts.size() < 4; ++i)
            if (usedIdx.insert(static_cast<uint32_t>(i)).second)
                hullVerts.push_back(static_cast<uint32_t>(i));
    }

    auto isCoplanar = [&](uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3) {
        Vec3 n = computeNormal(pts[i0], pts[i1], pts[i2]);
        return std::fabs(n.dot(pts[i3] - pts[i0])) < kVisibilityEpsilon;
    };

    uint32_t seed[4] = {};
    bool found = false;
    for (size_t a = 0; a + 3 < hullVerts.size() && !found; ++a)
        for (size_t b = a + 1; b + 2 < hullVerts.size() && !found; ++b)
            for (size_t c = b + 1; c + 1 < hullVerts.size() && !found; ++c)
                for (size_t d = c + 1; d < hullVerts.size() && !found; ++d) {
                    if (!isCoplanar(hullVerts[a], hullVerts[b], hullVerts[c], hullVerts[d])) {
                        seed[0] = hullVerts[a]; seed[1] = hullVerts[b];
                        seed[2] = hullVerts[c]; seed[3] = hullVerts[d];
                        found = true;
                    }
                }

    if (!found) return result;

    std::vector<HullFace> faces;
    auto addFace = [&](uint32_t v0, uint32_t v1, uint32_t v2) {
        Vec3 n = computeNormal(pts[v0], pts[v1], pts[v2]);
        float d = n.dot(pts[v0]);
        Vec3 center = pts[seed[0]] + pts[seed[1]] + pts[seed[2]] + pts[seed[3]];
        center = center * 0.25f;
        if (n.dot(center - pts[v0]) > 0.f) {
            n = n * -1.f;
            d = n.dot(pts[v0]);
        }
        faces.push_back({v0, v1, v2, n, d, false});
    };

    addFace(seed[0], seed[1], seed[2]);
    addFace(seed[0], seed[2], seed[3]);
    addFace(seed[0], seed[3], seed[1]);
    addFace(seed[1], seed[3], seed[2]);

    std::vector<uint32_t> remaining;
    for (size_t i = 0; i < n; ++i) {
        if (i == seed[0] || i == seed[1] || i == seed[2] || i == seed[3]) continue;
        remaining.push_back(static_cast<uint32_t>(i));
    }

    for (uint32_t pIdx : remaining) {
        const Vec3& P = pts[pIdx];

        std::vector<size_t> visibleFaces;
        for (size_t fi = 0; fi < faces.size(); ++fi) {
            if (faces[fi].removed) continue;
            float dist = distanceToPlane(faces[fi].normal, faces[fi].offset, P);
            if (dist > kVisibilityEpsilon) visibleFaces.push_back(fi);
        }

        if (visibleFaces.empty()) continue;

        std::set<std::pair<uint64_t, uint64_t>> horizon;
        for (size_t fi : visibleFaces) {
            faces[fi].removed = true;
            uint32_t verts[3] = {faces[fi].a, faces[fi].b, faces[fi].c};
            for (int e = 0; e < 3; ++e) {
                auto edge = makeEdge(verts[e], verts[(e + 1) % 3]);
                if (horizon.count(edge)) {
                    horizon.erase(edge);
                } else {
                    horizon.insert(edge);
                }
            }
        }

        for (auto& edge : horizon) {
            uint32_t e0 = static_cast<uint32_t>(edge.first);
            uint32_t e1 = static_cast<uint32_t>(edge.second);
            Vec3 n = computeNormal(pts[e0], pts[e1], P);
            Vec3 hullCenter = pts[seed[0]];
            if (n.dot(hullCenter - pts[e0]) > 0.f) {
                n = n * -1.f;
            }
            float d = n.dot(pts[e0]);
            faces.push_back({e0, e1, pIdx, n, d, false});
        }
    }

    std::unordered_map<uint32_t, uint32_t> vMap;
    std::vector<uint32_t> revMap;
    for (const auto& f : faces) {
        if (f.removed) continue;
        for (uint32_t vi : {f.a, f.b, f.c}) {
            if (vMap.find(vi) == vMap.end()) {
                vMap[vi] = static_cast<uint32_t>(result.vertices.size());
                revMap.push_back(vi);
                result.vertices.push_back(pts[vi]);
            }
        }
        result.faces.push_back(std::array<uint32_t, 3>{vMap[f.a], vMap[f.b], vMap[f.c]});
    }

    return result;
}

ConvexHull MeshConvexHull::fromMesh(const Mesh& mesh) {
    if (!mesh.isValid()) return {};
    return build(mesh.attributes().positions());
}

} // namespace nexus::geometry
