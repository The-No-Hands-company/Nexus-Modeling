#include <nexus/geometry/MeshBVH.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <bit>
#include <cmath>
#include <cstdint>
#include <queue>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr int32_t kMaxLeafTris = 8;
constexpr int32_t kMaxDepth = 64;
constexpr float kEpsilon = 1e-8f;

Vec3 vecMin(const Vec3& a, const Vec3& b) noexcept {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

Vec3 vecMax(const Vec3& a, const Vec3& b) noexcept {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

float pointToAabbDistSq(const Vec3& p, const Vec3& bMin, const Vec3& bMax) noexcept {
    float d2 = 0.f;
    float v;
    v = (p.x < bMin.x) ? (bMin.x - p.x) : ((p.x > bMax.x) ? (p.x - bMax.x) : 0.f);
    d2 += v * v;
    v = (p.y < bMin.y) ? (bMin.y - p.y) : ((p.y > bMax.y) ? (p.y - bMax.y) : 0.f);
    d2 += v * v;
    v = (p.z < bMin.z) ? (bMin.z - p.z) : ((p.z > bMax.z) ? (p.z - bMax.z) : 0.f);
    d2 += v * v;
    return d2;
}

double pointTriDistSq(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c,
                      float& outU, float& outV) noexcept {
    const double e0x = static_cast<double>(b.x - a.x);
    const double e0y = static_cast<double>(b.y - a.y);
    const double e0z = static_cast<double>(b.z - a.z);
    const double e1x = static_cast<double>(c.x - a.x);
    const double e1y = static_cast<double>(c.y - a.y);
    const double e1z = static_cast<double>(c.z - a.z);
    const double vx  = static_cast<double>(p.x - a.x);
    const double vy  = static_cast<double>(p.y - a.y);
    const double vz  = static_cast<double>(p.z - a.z);

    const double d00 = e0x * e0x + e0y * e0y + e0z * e0z;
    const double d01 = e0x * e1x + e0y * e1y + e0z * e1z;
    const double d11 = e1x * e1x + e1y * e1y + e1z * e1z;
    const double d20 = vx * e0x + vy * e0y + vz * e0z;
    const double d21 = vx * e1x + vy * e1y + vz * e1z;

    constexpr double kTriEps = 1e-30;
    const double denom = d00 * d11 - d01 * d01;

    double s, t;
    if (std::abs(denom) > kTriEps) {
        s = (d11 * d20 - d01 * d21) / denom;
        t = (d00 * d21 - d01 * d20) / denom;
    } else {
        s = t = 0.0;
    }

    if (s >= 0.0 && t >= 0.0 && s + t <= 1.0) {
        outU = static_cast<float>(s);
        outV = static_cast<float>(t);
        double cx = vx - s * e0x - t * e1x;
        double cy = vy - s * e0y - t * e1y;
        double cz = vz - s * e0z - t * e1z;
        return cx * cx + cy * cy + cz * cz;
    }

    double bestDist = std::numeric_limits<double>::max();
    double bestS = 0.0, bestT = 0.0;

    auto checkEdge = [&](double param, double ex, double ey, double ez, double sVal, double tVal) {
        double tClamp = std::clamp(param, 0.0, 1.0);
        double dx = vx - tClamp * ex;
        double dy = vy - tClamp * ey;
        double dz = vz - tClamp * ez;
        double dSq = dx * dx + dy * dy + dz * dz;
        if (dSq < bestDist) { bestDist = dSq; bestS = sVal; bestT = tVal; }
    };

    checkEdge(d20 / (d00 + kTriEps), e0x, e0y, e0z, 1.0, 0.0);
    {
        double clampedParam = std::clamp(d21 / (d11 + kTriEps), 0.0, 1.0);
        double dx = vx - clampedParam * e1x;
        double dy = vy - clampedParam * e1y;
        double dz = vz - clampedParam * e1z;
        double dSq = dx * dx + dy * dy + dz * dz;
        if (dSq < bestDist) { bestDist = dSq; bestS = 0.0; bestT = 1.0; }
    }
    {
        double ebx = e0x - e1x, eby = e0y - e1y, ebz = e0z - e1z;
        double d00b = ebx * ebx + eby * eby + ebz * ebz;
        double d20b = (vx - e1x) * ebx + (vy - e1y) * eby + (vz - e1z) * ebz;
        double clampedParam = std::clamp(d20b / (d00b + kTriEps), 0.0, 1.0);
        double dx = (vx - e1x) - clampedParam * ebx;
        double dy = (vy - e1y) - clampedParam * eby;
        double dz = (vz - e1z) - clampedParam * ebz;
        double dSq = dx * dx + dy * dy + dz * dz;
        if (dSq < bestDist) { bestDist = dSq; bestS = 1.0 - clampedParam; bestT = clampedParam; }
    }
    {
        double dSq = vx * vx + vy * vy + vz * vz;
        if (dSq < bestDist) { bestDist = dSq; bestS = 0.0; bestT = 0.0; }
    }
    {
        double dx = vx - e0x, dy = vy - e0y, dz = vz - e0z;
        double dSq = dx * dx + dy * dy + dz * dz;
        if (dSq < bestDist) { bestDist = dSq; bestS = 1.0; bestT = 0.0; }
    }
    {
        double dx = vx - e1x, dy = vy - e1y, dz = vz - e1z;
        double dSq = dx * dx + dy * dy + dz * dz;
        if (dSq < bestDist) { bestDist = dSq; bestS = 0.0; bestT = 1.0; }
    }

    outU = static_cast<float>(bestS);
    outV = static_cast<float>(bestT);
    return bestDist;
}

int32_t medianSplitAxis(const Vec3& extent) noexcept {
    if (extent.x >= extent.y && extent.x >= extent.z) return 0;
    if (extent.y >= extent.z) return 1;
    return 2;
}

// NaN/Inf guard: use bit_cast to bypass -ffast-math optimising away std::isnan.
[[nodiscard]] bool isFiniteVec3(const Vec3& v) noexcept {
    const std::uint32_t bx = std::bit_cast<std::uint32_t>(v.x);
    const std::uint32_t by = std::bit_cast<std::uint32_t>(v.y);
    const std::uint32_t bz = std::bit_cast<std::uint32_t>(v.z);
    return (bx & 0x7F800000u) != 0x7F800000u &&
           (by & 0x7F800000u) != 0x7F800000u &&
           (bz & 0x7F800000u) != 0x7F800000u;
}

} // namespace

void MeshBVH::build(const Mesh& mesh) {
    m_nodes.clear();
    m_tris.clear();
    m_valid = false;

    if (!mesh.isValid()) return;
    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();

    std::vector<BuildTri> buildTris;
    buildTris.reserve(topo.faceCount());

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& face = topo.face(fi);
        if (!face.indicesInBounds(pos.size())) continue;
        if (face.indices.size() < 3) continue;

        // Scan vertex positions for NaN/Inf; skip degenerate triangles.
        bool hasNans = false;
        for (size_t vi = 0; vi < face.indices.size(); ++vi) {
            if (!isFiniteVec3(pos[face.indices[vi]])) {
                hasNans = true;
                break;
            }
        }
        if (hasNans) {
            continue;
        }

        const Vec3& p0 = pos[face.indices[0]];
        Vec3 centroid = p0;
        Vec3 tMin = p0;
        Vec3 tMax = p0;

        for (size_t vi = 1; vi < face.indices.size(); ++vi) {
            const Vec3& p = pos[face.indices[vi]];
            centroid.x += p.x;
            centroid.y += p.y;
            centroid.z += p.z;
            tMin = vecMin(tMin, p);
            tMax = vecMax(tMax, p);
        }
        centroid.x /= static_cast<float>(face.indices.size());
        centroid.y /= static_cast<float>(face.indices.size());
        centroid.z /= static_cast<float>(face.indices.size());

        for (size_t vi = 0; vi + 2 < face.indices.size(); ++vi) {
            const Vec3& v0 = pos[face.indices[0]];
            const Vec3& v1 = pos[face.indices[vi + 1]];
            const Vec3& v2 = pos[face.indices[vi + 2]];

            Tri tri;
            tri.v0 = v0;
            tri.e1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
            tri.e2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
            tri.srcFace = static_cast<uint32_t>(fi);
            m_tris.push_back(tri);

            BuildTri bt;
            bt.centroid = tri.center();
            bt.aabbMin = vecMin(vecMin(v0, v1), v2);
            bt.aabbMax = vecMax(vecMax(v0, v1), v2);
            bt.srcFace = static_cast<uint32_t>(fi);
            buildTris.push_back(bt);
        }
    }

    if (buildTris.empty()) return;

    buildNode(buildTris, 0, static_cast<int32_t>(buildTris.size()));
    m_valid = true;
}

int32_t MeshBVH::buildNode(std::vector<BuildTri>& items, int32_t first, int32_t count) {
    if (count <= 0) return -1;

    Vec3 nodeMin = items[static_cast<size_t>(first)].aabbMin;
    Vec3 nodeMax = items[static_cast<size_t>(first)].aabbMax;
    for (int32_t i = 1; i < count; ++i) {
        const auto& it = items[static_cast<size_t>(first + i)];
        nodeMin = vecMin(nodeMin, it.aabbMin);
        nodeMax = vecMax(nodeMax, it.aabbMax);
    }

    int32_t nodeIdx = static_cast<int32_t>(m_nodes.size());
    m_nodes.emplace_back();
    m_nodes.back().min = nodeMin;
    m_nodes.back().max = nodeMax;

    if (count <= kMaxLeafTris || static_cast<int32_t>(m_nodes.size()) > kMaxDepth) {
        m_nodes.back().isLeaf = true;
        m_nodes.back().firstTri = first;
        m_nodes.back().triCount = count;
        m_nodes.back().leftChild = -1;
        return nodeIdx;
    }

    Vec3 extent = {nodeMax.x - nodeMin.x, nodeMax.y - nodeMin.y, nodeMax.z - nodeMin.z};
    int32_t axis = medianSplitAxis(extent);

    auto midIt = items.begin() + first + count / 2;
    std::nth_element(items.begin() + first, midIt, items.begin() + first + count,
                     [axis](const BuildTri& a, const BuildTri& b) {
                         if (axis == 0) return a.centroid.x < b.centroid.x;
                         if (axis == 1) return a.centroid.y < b.centroid.y;
                         return a.centroid.z < b.centroid.z;
                     });

    int32_t mid = count / 2;
    int32_t leftIdx = buildNode(items, first, mid);
    int32_t rightIdx = buildNode(items, first + mid, count - mid);

    m_nodes[static_cast<size_t>(nodeIdx)].leftChild = leftIdx;
    m_nodes[static_cast<size_t>(nodeIdx)].firstTri = rightIdx;

    return nodeIdx;
}

bool MeshBVH::intersectAabb(const Node& node, const Ray& ray, float& tMin, float& tMax) noexcept {
    tMin = 0.f;
    tMax = std::numeric_limits<float>::max();

    float invDx = std::fabs(ray.direction.x) < kEpsilon ? 1e30f : 1.f / ray.direction.x;
    float t1 = (node.min.x - ray.origin.x) * invDx;
    float t2 = (node.max.x - ray.origin.x) * invDx;
    tMin = std::max(tMin, std::min(t1, t2));
    tMax = std::min(tMax, std::max(t1, t2));
    if (tMin > tMax) return false;

    float invDy = std::fabs(ray.direction.y) < kEpsilon ? 1e30f : 1.f / ray.direction.y;
    t1 = (node.min.y - ray.origin.y) * invDy;
    t2 = (node.max.y - ray.origin.y) * invDy;
    tMin = std::max(tMin, std::min(t1, t2));
    tMax = std::min(tMax, std::max(t1, t2));
    if (tMin > tMax) return false;

    float invDz = std::fabs(ray.direction.z) < kEpsilon ? 1e30f : 1.f / ray.direction.z;
    t1 = (node.min.z - ray.origin.z) * invDz;
    t2 = (node.max.z - ray.origin.z) * invDz;
    tMin = std::max(tMin, std::min(t1, t2));
    tMax = std::min(tMax, std::max(t1, t2));
    if (tMin > tMax) return false;

    return true;
}

bool MeshBVH::intersectTri(const Tri& tri, const Ray& ray, float& t, float& u, float& v) noexcept {
    const Vec3 h = ray.direction.cross(tri.e2);
    float det = tri.e1.dot(h);
    if (std::fabs(det) < kEpsilon) return false;

    float invDet = 1.f / det;
    const Vec3 s = {ray.origin.x - tri.v0.x, ray.origin.y - tri.v0.y, ray.origin.z - tri.v0.z};
    float b1 = s.dot(h) * invDet;
    if (b1 < 0.f || b1 > 1.f) return false;

    const Vec3 q = s.cross(tri.e1);
    float b2 = ray.direction.dot(q) * invDet;
    if (b2 < 0.f || b1 + b2 > 1.f) return false;

    float tt = tri.e2.dot(q) * invDet;
    if (tt < 0.f) return false;

    t = tt;
    u = b1;
    v = b2;
    return true;
}

RayHit MeshBVH::raycast(const Ray& ray) const noexcept {
    RayHit bestHit;

    if (!m_valid || m_nodes.empty()) return bestHit;

    struct StackEntry {
        int32_t nodeIdx;
        float tMin;
    };

    static constexpr size_t kStackReserve = 256;
    std::vector<StackEntry> stack;
    stack.reserve(kStackReserve);
    stack.push_back({0, 0.f});

    while (!stack.empty()) {
        StackEntry entry = stack.back();
        stack.pop_back();
        const Node& node = m_nodes[static_cast<size_t>(entry.nodeIdx)];

        float tMin, tMax;
        if (!intersectAabb(node, ray, tMin, tMax) || tMin >= bestHit.t) continue;

        if (node.isLeaf) {
            for (int32_t i = 0; i < node.triCount; ++i) {
                const Tri& tri = m_tris[static_cast<size_t>(node.firstTri + i)];
                float tt, uu, vv;
                if (intersectTri(tri, ray, tt, uu, vv) && tt < bestHit.t) {
                    bestHit.t = tt;
                    bestHit.triangleIndex = tri.srcFace;
                    bestHit.u = uu;
                    bestHit.v = vv;
                }
            }
        } else {
            if (node.leftChild >= 0) {
                stack.push_back({node.leftChild, tMin});
            }
            int32_t rightChild = node.firstTri;
            if (rightChild >= 0) {
                stack.push_back({rightChild, tMin});
            }
            // With a 64-depth limit, the stack should never exceed 256 entries.
            // In debug builds, assert this invariant.
            assert(stack.size() <= kStackReserve && "BVH stack overflow — depth > 64?");
        }
    }

    return bestHit;
}

bool MeshBVH::isValid() const noexcept {
    return m_valid && !m_nodes.empty();
}

ClosestPointHit MeshBVH::closestPoint(const Vec3& query) const noexcept {
    ClosestPointHit best;
    if (!m_valid || m_nodes.empty()) return best;

    struct HeapEntry {
        int32_t nodeIdx;
        float distSq;
    };

    auto cmp = [](const HeapEntry& a, const HeapEntry& b) { return a.distSq > b.distSq; };
    std::priority_queue<HeapEntry, std::vector<HeapEntry>, decltype(cmp)> heap(cmp);
    heap.push({0, pointToAabbDistSq(query, m_nodes[0].min, m_nodes[0].max)});

    float bestDistSq = std::numeric_limits<float>::max();
    int32_t iterations = 0;
    int32_t maxIterations = static_cast<int32_t>(m_nodes.size() + m_tris.size()) * 4;

    while (!heap.empty()) {
        if (++iterations > maxIterations) break;

        HeapEntry entry = heap.top();
        heap.pop();

        if (entry.distSq >= bestDistSq) continue;
        if (entry.nodeIdx < 0 || static_cast<size_t>(entry.nodeIdx) >= m_nodes.size()) continue;

        const Node& node = m_nodes[static_cast<size_t>(entry.nodeIdx)];

        if (node.isLeaf) {
            for (int32_t i = 0; i < node.triCount; ++i) {
                size_t triIdx = static_cast<size_t>(node.firstTri + i);
                if (triIdx >= m_tris.size()) continue;
                const Tri& tri = m_tris[triIdx];
                const Vec3 b = {tri.v0.x + tri.e1.x, tri.v0.y + tri.e1.y, tri.v0.z + tri.e1.z};
                const Vec3 c = {tri.v0.x + tri.e2.x, tri.v0.y + tri.e2.y, tri.v0.z + tri.e2.z};

                float u, v;
                double dSq = pointTriDistSq(query, tri.v0, b, c, u, v);
                float dSqF = static_cast<float>(dSq);
                if (dSqF < bestDistSq) {
                    bestDistSq = dSqF;
                    best.distanceSquared = dSqF;
                    best.triangleIndex = tri.srcFace;
                    best.baryU = u;
                    best.baryV = v;
                    best.point = {
                        tri.v0.x + u * tri.e1.x + v * tri.e2.x,
                        tri.v0.y + u * tri.e1.y + v * tri.e2.y,
                        tri.v0.z + u * tri.e1.z + v * tri.e2.z,
                    };
                }
            }
        } else {
            if (node.leftChild >= 0) {
                size_t lc = static_cast<size_t>(node.leftChild);
                if (lc < m_nodes.size()) {
                    float d = pointToAabbDistSq(query, m_nodes[lc].min, m_nodes[lc].max);
                    if (d < bestDistSq) heap.push({node.leftChild, d});
                }
            }
            int32_t rightChild = node.firstTri;
            if (rightChild >= 0) {
                size_t rc = static_cast<size_t>(rightChild);
                if (rc < m_nodes.size()) {
                    float d = pointToAabbDistSq(query, m_nodes[rc].min, m_nodes[rc].max);
                    if (d < bestDistSq) heap.push({rightChild, d});
                }
            }
        }
    }

    if (bestDistSq < std::numeric_limits<float>::max()) best.valid = true;
    return best;
}

} // namespace nexus::geometry
