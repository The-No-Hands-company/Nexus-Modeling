#include <nexus/geometry/MeshAnalysis.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

Vec3 faceNormal(const Mesh& mesh, uint32_t fi) {
    const auto& f = mesh.topology().face(fi);
    if (f.vertexCount() < 3) return {0, 0, 1};
    const auto& pos = mesh.attributes().positions();
    Vec3 a = pos[f.indices[0]];
    Vec3 b = pos[f.indices[1]];
    Vec3 c = pos[f.indices[2]];
    Vec3 n = (b - a).cross(c - a);
    float len = n.length();
    return (len > 1e-10f) ? n * (1.f / len) : Vec3{0, 0, 1};
}

bool triTriIntersect(const Vec3& a0, const Vec3& a1, const Vec3& a2,
                     const Vec3& b0, const Vec3& b1, const Vec3& b2) {
    // Separating-axis test using face normals and edge cross products.
    Vec3 nA = (a1 - a0).cross(a2 - a0);
    Vec3 nB = (b1 - b0).cross(b2 - b0);
    float lA = nA.length();
    float lB = nB.length();
    if (lA < 1e-10f || lB < 1e-10f) return false;
    nA = nA * (1.f / lA);
    nB = nB * (1.f / lB);

    // Test separation by plane A.
    float dA0 = nA.dot(a0);
    float dB0 = nA.dot(b0);
    float dB1 = nA.dot(b1);
    float dB2 = nA.dot(b2);
    float minB = std::min({dB0, dB1, dB2});
    float maxB = std::max({dB0, dB1, dB2});
    if (dA0 > maxB + 1e-8f || dA0 < minB - 1e-8f) return false;

    // Test separation by plane B.
    float dB = nB.dot(b0);
    float da0 = nB.dot(a0), da1 = nB.dot(a1), da2 = nB.dot(a2);
    float minA = std::min({da0, da1, da2});
    float maxA = std::max({da0, da1, da2});
    if (dB > maxA + 1e-8f || dB < minA - 1e-8f) return false;

    // Test edge-edge cross product separating axes.
    Vec3 edgesA[3] = {a1 - a0, a2 - a1, a0 - a2};
    Vec3 edgesB[3] = {b1 - b0, b2 - b1, b0 - b2};

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Vec3 axis = edgesA[i].cross(edgesB[j]);
            float axLen = axis.length();
            if (axLen < 1e-10f) continue;
            axis = axis * (1.f / axLen);

            float pa0 = axis.dot(a0), pa1 = axis.dot(a1), pa2 = axis.dot(a2);
            float pb0 = axis.dot(b0), pb1 = axis.dot(b1), pb2 = axis.dot(b2);
            float minPA = std::min({pa0, pa1, pa2}), maxPA = std::max({pa0, pa1, pa2});
            float minPB = std::min({pb0, pb1, pb2}), maxPB = std::max({pb0, pb1, pb2});
            if (maxPA < minPB - 1e-8f || maxPB < minPA - 1e-8f) return false;
        }
    }

    return true;
}

} // namespace

// ── Collision ──────────────────────────────────────────────────────────────

namespace {

bool aabbOverlap(const MeshBVH::Node& a, const MeshBVH::Node& b) {
    return a.max.x >= b.min.x && b.max.x >= a.min.x &&
           a.max.y >= b.min.y && b.max.y >= a.min.y &&
           a.max.z >= b.min.z && b.max.z >= a.min.z;
}

void traverseBVH(int32_t nodeA, int32_t nodeB,
                 const MeshBVH& bvhA, const MeshBVH& bvhB,
                 CollisionResult& result) {
    if (nodeA < 0 || nodeB < 0) return;
    const auto& nA = bvhA.nodes()[static_cast<size_t>(nodeA)];
    const auto& nB = bvhB.nodes()[static_cast<size_t>(nodeB)];
    if (!aabbOverlap(nA, nB)) return;

    if (nA.isLeaf && nB.isLeaf) {
        const auto& trisA = bvhA.tris();
        const auto& trisB = bvhB.tris();
        for (int32_t ti = nA.firstTri; ti < nA.firstTri + nA.triCount; ++ti) {
            for (int32_t tj = nB.firstTri; tj < nB.firstTri + nB.triCount; ++tj) {
                Vec3 a0 = trisA[ti].v0;
                Vec3 a1 = a0 + trisA[ti].e1;
                Vec3 a2 = a0 + trisA[ti].e2;
                Vec3 b0 = trisB[tj].v0;
                Vec3 b1 = b0 + trisB[tj].e1;
                Vec3 b2 = b0 + trisB[tj].e2;
                if (triTriIntersect(a0, a1, a2, b0, b1, b2)) {
                    result.colliding = true;
                    result.trianglePairs.emplace_back(trisA[ti].srcFace, trisB[tj].srcFace);
                }
            }
        }
        return;
    }

    if (!nA.isLeaf && !nB.isLeaf) {
        float volA = (nA.max.x - nA.min.x) * (nA.max.y - nA.min.y) * (nA.max.z - nA.min.z);
        float volB = (nB.max.x - nB.min.x) * (nB.max.y - nB.min.y) * (nB.max.z - nB.min.z);
        if (volA > volB) {
            traverseBVH(nA.leftChild, nodeB, bvhA, bvhB, result);
            traverseBVH(nA.leftChild + 1, nodeB, bvhA, bvhB, result);
        } else {
            traverseBVH(nodeA, nB.leftChild, bvhA, bvhB, result);
            traverseBVH(nodeA, nB.leftChild + 1, bvhA, bvhB, result);
        }
    } else if (!nA.isLeaf) {
        traverseBVH(nA.leftChild, nodeB, bvhA, bvhB, result);
        traverseBVH(nA.leftChild + 1, nodeB, bvhA, bvhB, result);
    } else {
        traverseBVH(nodeA, nB.leftChild, bvhA, bvhB, result);
        traverseBVH(nodeA, nB.leftChild + 1, bvhA, bvhB, result);
    }
}

} // namespace

CollisionResult meshMeshCollision(const Mesh& a, const Mesh& b)
{
    CollisionResult result;
    if (!a.topology().hasValidIndices(a.attributes().vertexCount()) ||
        !b.topology().hasValidIndices(b.attributes().vertexCount()))
        return result;

    MeshBVH bvhA, bvhB;
    bvhA.build(a);
    bvhB.build(b);
    if (!bvhA.isValid() || !bvhB.isValid()) return result;

    if (!bvhA.nodes().empty() && !bvhB.nodes().empty())
        traverseBVH(0, 0, bvhA, bvhB, result);
    return result;
}

// ── Clearance ──────────────────────────────────────────────────────────────

ClearanceResult meshMeshClearance(const Mesh& a, const Mesh& b)
{
    ClearanceResult result;
    result.distance = std::numeric_limits<float>::max();
    const auto& posA = a.attributes().positions();
    const auto& posB = b.attributes().positions();

    // Sample closest vertex-vertex distance.
    size_t stepA = std::max(size_t(1), posA.size() / 1000);
    size_t stepB = std::max(size_t(1), posB.size() / 1000);

    for (size_t i = 0; i < posA.size(); i += stepA) {
        for (size_t j = 0; j < posB.size(); j += stepB) {
            float dx = posA[i].x - posB[j].x;
            float dy = posA[i].y - posB[j].y;
            float dz = posA[i].z - posB[j].z;
            float d = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (d < result.distance) {
                result.distance = d;
                result.pointA = posA[i];
                result.pointB = posB[j];
            }
        }
    }
    return result;
}

// ── Draft Angle ────────────────────────────────────────────────────────────

DraftAngleResult analyzeDraftAngle(const Mesh& mesh, const Vec3& pullDirection)
{
    DraftAngleResult result;
    Vec3 pull = pullDirection.normalize();

    uint32_t fc = mesh.topology().faceCount();
    result.perFaceAngles.reserve(fc);
    double sum = 0.0;
    result.minAngle = 360.f;

    for (uint32_t fi = 0; fi < fc; ++fi) {
        Vec3 n = faceNormal(mesh, fi);
        float dot = std::abs(n.dot(pull)); // |cos(angle with pull)|
        float angleDeg = 90.f - std::asin(std::min(dot, 1.f)) * 180.f / static_cast<float>(std::numbers::pi);
        result.perFaceAngles.push_back(angleDeg);
        result.minAngle = std::min(result.minAngle, angleDeg);
        result.maxAngle = std::max(result.maxAngle, angleDeg);
        sum += angleDeg;
    }
    result.avgAngle = fc > 0 ? static_cast<float>(sum / fc) : 0.f;
    return result;
}

// ── Thickness ──────────────────────────────────────────────────────────────

ThicknessResult analyzeThickness(const Mesh& mesh, uint32_t /*rayCount*/)
{
    ThicknessResult result;
    const auto& pos = mesh.attributes().positions();
    result.perVertexThickness.resize(pos.size(), 0.f);

    // Pre-build vertex→face adjacency for faster normal computation.
    uint32_t vc = mesh.attributes().vertexCount();
    uint32_t fc = mesh.topology().faceCount();
    std::vector<std::vector<uint32_t>> vertexFaces(vc);
    for (uint32_t fi = 0; fi < fc; ++fi) {
        const auto& f = mesh.topology().face(fi);
        for (size_t k = 0; k < f.vertexCount(); ++k)
            if (f.indices[k] < vc)
                vertexFaces[f.indices[k]].push_back(fi);
    }

    // Subsample: don't check every vertex pair (O(n²)). Use random sampling.
    size_t sampleCount = std::min(pos.size(), size_t(200));
    size_t step = std::max(size_t(1), pos.size() / sampleCount);

    for (size_t i = 0; i < pos.size(); i += step) {
        Vec3 vi = pos[i];
        float minDist = std::numeric_limits<float>::max();

        Vec3 avgNormal{0, 0, 0};
        uint32_t nCount = 0;
        for (uint32_t faceIdx : vertexFaces[i]) {
            if (nCount >= 10) break;
            avgNormal = avgNormal + faceNormal(mesh, faceIdx);
            nCount++;
        }
        if (nCount > 0) avgNormal = avgNormal * (1.f / static_cast<float>(nCount));
        else continue;

        for (size_t j = 0; j < pos.size(); ++j) {
            if (j == i) continue;
            Vec3 d = pos[j] - vi;
            float dist = d.length();
            if (dist < 1e-10f) continue;
            Vec3 dir = d * (1.f / dist);
            if (avgNormal.dot(dir) < -0.3f) {
                if (dist < minDist) minDist = dist;
            }
        }
        result.perVertexThickness[i] = (minDist < std::numeric_limits<float>::max()) ? minDist : 0.f;
    }

    if (!pos.empty()) {
        float sum = 0.f;
        uint32_t validCount = 0;
        result.minThickness = std::numeric_limits<float>::max();
        for (float t : result.perVertexThickness) {
            if (t > 0.f) {
                sum += t;
                validCount++;
                result.minThickness = std::min(result.minThickness, t);
                result.maxThickness = std::max(result.maxThickness, t);
            }
        }
        result.avgThickness = (validCount > 0) ? sum / static_cast<float>(validCount) : 0.f;
    }
    return result;
}

} // namespace nexus::geometry
