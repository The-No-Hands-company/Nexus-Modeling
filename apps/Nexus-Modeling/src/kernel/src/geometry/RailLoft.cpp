#include <nexus/geometry/RailLoft.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh RailLoft::loft(const NurbsCurve& rail0,
                      const NurbsCurve& rail1,
                      const ProfileLoop& profile,
                      const RailLoftOptions& opts) {
    Mesh result;
    auto& attrs = result.attributes();
    auto& topo  = result.topology();

    if (!rail0.isValid() || !rail1.isValid() || profile.points.size() < 3) return result;

    auto dom0 = rail0.domain();
    auto dom1 = rail1.domain();
    int32_t samples = std::max(opts.samplesPerRail, 2);

    int32_t profSize = static_cast<int32_t>(profile.points.size());
    int32_t profSamples = opts.closedProfile ? profSize : std::min(profSize, opts.profileSamples);

    std::vector<Vec3> positions;
    positions.reserve(static_cast<size_t>(samples * profSamples));

    for (int32_t i = 0; i < samples; ++i) {
        float t0 = dom0.first + (dom0.second - dom0.first) * static_cast<float>(i) / static_cast<float>(samples - 1);
        float t1 = dom1.first + (dom1.second - dom1.first) * static_cast<float>(i) / static_cast<float>(samples - 1);

        Vec3 p0 = rail0.evaluate(t0);
        Vec3 p1 = rail1.evaluate(t1);

        Vec3 railDir0 = rail0.derivative(t0);
        Vec3 railDir1 = rail1.derivative(t1);
        Vec3 railAvg = (railDir0.normalize() + railDir1.normalize()).normalize();

        Vec3 worldUp{0.f, 1.f, 0.f};
        float upDot = railAvg.dot(worldUp);
        if (std::abs(upDot) > 0.999f) worldUp = Vec3{1.f, 0.f, 0.f};

        Vec3 side = railAvg.cross(worldUp);
        float sideLen = side.length();
        if (sideLen < 1e-8f) side = Vec3{1.f, 0.f, 0.f};
        else side = side * (1.0f / sideLen);

        Vec3 up = side.cross(railAvg).normalize();

        float span = (p1 - p0).length();

        for (int32_t j = 0; j < profSamples; ++j) {
            float t = static_cast<float>(j) / static_cast<float>(profSamples - 1);
            int32_t pidx = static_cast<int32_t>(t * (profSize - 1) + 0.5f);
            pidx = std::clamp(pidx, 0, profSize - 1);
            const Vec3& profilePt = profile.points[static_cast<size_t>(pidx)];

            Vec3 pos = p0 + side * profilePt.x * span * 0.5f + up * profilePt.y;
            positions.push_back(pos);
        }
    }

    for (int32_t i = 0; i < samples - 1; ++i) {
        for (int32_t j = 0; j < profSamples - 1; ++j) {
            uint32_t a = static_cast<uint32_t>(i * profSamples + j);
            uint32_t b = static_cast<uint32_t>(i * profSamples + j + 1);
            uint32_t c = static_cast<uint32_t>((i + 1) * profSamples + j + 1);
            uint32_t d = static_cast<uint32_t>((i + 1) * profSamples + j);
            Face f1; f1.indices = {a, b, c}; topo.addFace(f1);
            Face f2; f2.indices = {a, c, d}; topo.addFace(f2);
        }
    }

    attrs.setPositions(positions);
    if (!result.computeVertexNormals()) return result;
    return result;
}

} // namespace nexus::geometry
