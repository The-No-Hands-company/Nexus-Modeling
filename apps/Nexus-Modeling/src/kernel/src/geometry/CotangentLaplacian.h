#pragma once

#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nexus::geometry::internal {

using Vec3 = nexus::render::Vec3;

struct FanEntry {
    uint32_t to = 0u;
    float    cot = 0.f;
};

inline void buildCotangentFan(const nexus::geometry::Mesh& mesh,
                              std::vector<std::vector<FanEntry>>& fan,
                              std::vector<float>& areas) noexcept {
    const auto& topology   = mesh.topology();
    const auto& positions  = mesh.attributes().positions();
    const size_t vertCount = positions.size();
    const size_t faceCount = topology.faceCount();

    fan.resize(vertCount);
    for (auto& f : fan) {
        f.clear();
    }
    areas.assign(vertCount, 0.f);

    std::vector<float> faceAreas(faceCount, 0.f);

    for (size_t fi = 0; fi < faceCount; ++fi) {
        const auto& face = topology.face(fi);
        const size_t nv = face.vertexCount();
        if (nv < 3) continue;

        for (size_t vi = 0; vi < nv; ++vi) {
            const uint32_t idxPrev = face.indices[(vi + nv - 1) % nv];
            const uint32_t idxCurr = face.indices[vi];
            const uint32_t idxNext = face.indices[(vi + 1) % nv];

            const Vec3& pPrev = positions[idxPrev];
            const Vec3& pCurr = positions[idxCurr];
            const Vec3& pNext = positions[idxNext];

            const float ex = pPrev.x - pCurr.x;
            const float ey = pPrev.y - pCurr.y;
            const float ez = pPrev.z - pCurr.z;
            const float aLenSq = ex * ex + ey * ey + ez * ez;

            const float fx = pNext.x - pCurr.x;
            const float fy = pNext.y - pCurr.y;
            const float fz = pNext.z - pCurr.z;
            const float bLenSq = fx * fx + fy * fy + fz * fz;

            const float gx = pNext.x - pPrev.x;
            const float gy = pNext.y - pPrev.y;
            const float gz = pNext.z - pPrev.z;
            const float cLenSq = gx * gx + gy * gy + gz * gz;

            const float crossX = ex * fz - ez * fy;
            const float crossY = ez * fx - ex * fz;
            const float crossZ = ex * fy - ey * fx;

            const float area2x = 0.5f * std::sqrt(crossX * crossX +
                                                   crossY * crossY +
                                                   crossZ * crossZ);

            if (area2x < 1e-10f) continue;

            const float cosAngle = (aLenSq + bLenSq - cLenSq);
            const float sinAngle = 2.0f * area2x;

            const float cotVal = cosAngle / (sinAngle + 1e-30f);

            fan[idxPrev].push_back(FanEntry{idxNext, cotVal});
            fan[idxNext].push_back(FanEntry{idxPrev, cotVal});

            const float triAreaThird = area2x / 3.0f;
            faceAreas[fi] += triAreaThird;
            areas[idxPrev] += triAreaThird;
            areas[idxCurr] += triAreaThird;
            areas[idxNext] += triAreaThird;
        }
    }
}

} // namespace nexus::geometry::internal
