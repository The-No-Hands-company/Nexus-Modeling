#include <nexus/geometry/MeshSelfIntersection.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEpsilon = 1e-20f;

struct AabbBox {
    Vec3 min, max;
};

bool overlapAabb(const AabbBox& a, const AabbBox& b) noexcept {
    if (a.max.x < b.min.x || b.max.x < a.min.x) return false;
    if (a.max.y < b.min.y || b.max.y < a.min.y) return false;
    if (a.max.z < b.min.z || b.max.z < a.min.z) return false;
    return true;
}

bool moellerTriTriOverlap(const Vec3& a0, const Vec3& a1, const Vec3& a2,
                           const Vec3& b0, const Vec3& b1, const Vec3& b2) noexcept {
    const Vec3 triA[3] = {a0, a1, a2};
    const Vec3 triB[3] = {b0, b1, b2};

    Vec3 nA = (Vec3{a1.x - a0.x, a1.y - a0.y, a1.z - a0.z}).cross(
              Vec3{a2.x - a0.x, a2.y - a0.y, a2.z - a0.z});
    Vec3 nB = (Vec3{b1.x - b0.x, b1.y - b0.y, b1.z - b0.z}).cross(
              Vec3{b2.x - b0.x, b2.y - b0.y, b2.z - b0.z});

    float dA0 = nA.dot(a0);
    float dB0 = nB.dot(b0);

    float minA, maxA, minB, maxB;
    {
        float d0 = nA.dot(b0), d1 = nA.dot(b1), d2 = nA.dot(b2);
        minB = std::min({d0, d1, d2});
        maxB = std::max({d0, d1, d2});
        if (maxB < dA0 || minB > dA0) return false;
    }
    {
        float d0 = nB.dot(a0), d1 = nB.dot(a1), d2 = nB.dot(a2);
        minA = std::min({d0, d1, d2});
        maxA = std::max({d0, d1, d2});
        if (maxA < dB0 || minA > dB0) return false;
    }

    for (int ei = 0; ei < 3; ++ei) {
        const Vec3& e0 = triA[ei];
        const Vec3& e1 = triA[(ei + 1) % 3];
        Vec3 edge = {e1.x - e0.x, e1.y - e0.y, e1.z - e0.z};

        for (int ej = 0; ej < 3; ++ej) {
            const Vec3& f0 = triB[ej];
            const Vec3& f1 = triB[(ej + 1) % 3];
            Vec3 fedg = {f1.x - f0.x, f1.y - f0.y, f1.z - f0.z};

            Vec3 crossN = edge.cross(fedg);
            float lenSq = crossN.lengthSq();

            if (lenSq < kEpsilon) continue;

            Vec3 cnNorm = {crossN.x / std::sqrt(lenSq), crossN.y / std::sqrt(lenSq), crossN.z / std::sqrt(lenSq)};

            float pA0 = cnNorm.dot(a0); float pA1 = cnNorm.dot(a1); float pA2 = cnNorm.dot(a2);
            minA = std::min({pA0, pA1, pA2});
            maxA = std::max({pA0, pA1, pA2});
            float pB0 = cnNorm.dot(b0); float pB1 = cnNorm.dot(b1); float pB2 = cnNorm.dot(b2);
            minB = std::min({pB0, pB1, pB2});
            maxB = std::max({pB0, pB1, pB2});

            if (maxA < minB || maxB < minA) return false;
        }
    }

    return true;
}

bool shareVertex(const Face& a, const Face& b) noexcept {
    if (a.indices.size() < 3 || b.indices.size() < 3) return false;
    for (auto va : a.indices) {
        for (auto vb : b.indices) {
            if (va == vb) return true;
        }
    }
    return false;
}

uint64_t cellKey(int32_t cx, int32_t cy, int32_t cz) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 42) |
           (static_cast<uint64_t>(static_cast<uint32_t>(cy)) << 21) |
           (static_cast<uint64_t>(static_cast<uint32_t>(cz)));
}

} // namespace

SelfIntersectionResult MeshSelfIntersection::detect(const Mesh& mesh) {
    SelfIntersectionResult result;
    if (!mesh.isValid()) return result;

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    const size_t fc = topo.faceCount();
    if (fc < 2) return result;

    struct FaceBox {
        AabbBox box;
        uint32_t idx;
    };
    std::vector<FaceBox> boxes;
    boxes.reserve(fc);

    Vec3 globalMin = pos[topo.face(0).indices[0]];
    Vec3 globalMax = globalMin;

    for (size_t fi = 0; fi < fc; ++fi) {
        const Face& face = topo.face(fi);
        if (face.indices.size() < 3) continue;
        const Vec3& p0 = pos[face.indices[0]];

        AabbBox b = {p0, p0};
        for (auto vi : face.indices) {
            const Vec3& p = pos[vi];
            b.min.x = std::min(b.min.x, p.x);
            b.min.y = std::min(b.min.y, p.y);
            b.min.z = std::min(b.min.z, p.z);
            b.max.x = std::max(b.max.x, p.x);
            b.max.y = std::max(b.max.y, p.y);
            b.max.z = std::max(b.max.z, p.z);
        }
        globalMin.x = std::min(globalMin.x, b.min.x);
        globalMin.y = std::min(globalMin.y, b.min.y);
        globalMin.z = std::min(globalMin.z, b.min.z);
        globalMax.x = std::max(globalMax.x, b.max.x);
        globalMax.y = std::max(globalMax.y, b.max.y);
        globalMax.z = std::max(globalMax.z, b.max.z);
        boxes.push_back({b, static_cast<uint32_t>(fi)});
    }

    if (boxes.size() < 2) return result;

    Vec3 diag = {globalMax.x - globalMin.x, globalMax.y - globalMin.y, globalMax.z - globalMin.z};
    if (diag.x < kEpsilon && diag.y < kEpsilon && diag.z < kEpsilon) {
        return result;
    }

    uint32_t gridDim = static_cast<uint32_t>(std::cbrt(static_cast<double>(boxes.size()))) * 2;
    if (gridDim < 4) gridDim = 4;
    if (gridDim > 128) gridDim = 128;

    float cellSizeX = diag.x / static_cast<float>(gridDim);
    float cellSizeY = diag.y / static_cast<float>(gridDim);
    float cellSizeZ = diag.z / static_cast<float>(gridDim);
    if (cellSizeX < kEpsilon) cellSizeX = 0.01f;
    if (cellSizeY < kEpsilon) cellSizeY = 0.01f;
    if (cellSizeZ < kEpsilon) cellSizeZ = 0.01f;

    std::unordered_map<uint64_t, std::vector<uint32_t>> grid;
    grid.reserve(boxes.size() * 3);

    for (size_t bi = 0; bi < boxes.size(); ++bi) {
        const auto& b = boxes[bi];
        int32_t cxMin = static_cast<int32_t>((b.box.min.x - globalMin.x) / cellSizeX);
        int32_t cyMin = static_cast<int32_t>((b.box.min.y - globalMin.y) / cellSizeY);
        int32_t czMin = static_cast<int32_t>((b.box.min.z - globalMin.z) / cellSizeZ);
        int32_t cxMax = static_cast<int32_t>((b.box.max.x - globalMin.x) / cellSizeX);
        int32_t cyMax = static_cast<int32_t>((b.box.max.y - globalMin.y) / cellSizeY);
        int32_t czMax = static_cast<int32_t>((b.box.max.z - globalMin.z) / cellSizeZ);

        cxMin = std::max(0, cxMin); cxMax = std::min(static_cast<int32_t>(gridDim - 1), cxMax);
        cyMin = std::max(0, cyMin); cyMax = std::min(static_cast<int32_t>(gridDim - 1), cyMax);
        czMin = std::max(0, czMin); czMax = std::min(static_cast<int32_t>(gridDim - 1), czMax);

        for (int32_t cz = czMin; cz <= czMax; ++cz) {
            for (int32_t cy = cyMin; cy <= cyMax; ++cy) {
                for (int32_t cx = cxMin; cx <= cxMax; ++cx) {
                    grid[cellKey(cx, cy, cz)].push_back(static_cast<uint32_t>(bi));
                }
            }
        }
    }

    std::vector<uint8_t> checked(boxes.size() * boxes.size(), 0);

    for (const auto& [key, cellFaces] : grid) {
        for (size_t ii = 0; ii < cellFaces.size(); ++ii) {
            uint32_t bi = cellFaces[ii];
            for (size_t jj = ii + 1; jj < cellFaces.size(); ++jj) {
                uint32_t bj = cellFaces[jj];
                if (bi == bj) continue;

                size_t pairIdx = static_cast<size_t>(bi) * boxes.size() + bj;
                size_t altIdx = static_cast<size_t>(bj) * boxes.size() + bi;
                if (checked[pairIdx] || checked[altIdx]) continue;
                checked[pairIdx] = 1;
                checked[altIdx] = 1;

                const Face& fa = topo.face(boxes[bi].idx);
                const Face& fb = topo.face(boxes[bj].idx);

                if (shareVertex(fa, fb)) continue;
                if (!overlapAabb(boxes[bi].box, boxes[bj].box)) continue;

                const Vec3& a0 = pos[fa.indices[0]];
                const Vec3& a1 = pos[fa.indices[1]];
                const Vec3& a2 = pos[fa.indices[2]];
                const Vec3& b0 = pos[fb.indices[0]];
                const Vec3& b1 = pos[fb.indices[1]];
                const Vec3& b2 = pos[fb.indices[2]];

                if (moellerTriTriOverlap(a0, a1, a2, b0, b1, b2)) {
                    result.intersections.push_back({boxes[bi].idx, boxes[bj].idx});
                }
            }
        }
    }

    return result;
}

} // namespace nexus::geometry
