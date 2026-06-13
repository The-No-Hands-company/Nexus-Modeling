#include <nexus/geometry/MeshHausdorffDistance.h>
#include <nexus/geometry/MeshPointSample.h>
#include <nexus/geometry/MeshBVH.h>

#include <algorithm>
#include <cmath>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

float nearestDist(const MeshBVH& bvh, const Vec3& point) {
    auto hit = bvh.closestPoint(point);
    if (!hit.valid) return 0.f;
    return std::sqrt(hit.distanceSquared);
}

} // namespace

HausdorffResult MeshHausdorffDistance::compute(
    const Mesh& a, const Mesh& b,
    const HausdorffOptions& opts) {

    HausdorffResult result;
    if (!a.isValid() || !b.isValid()) return result;

    PointSampleOptions sampOpts;
    sampOpts.count = opts.sampleCount;
    sampOpts.seed = opts.seed;

    auto samplesA = MeshPointSample::sample(a, sampOpts);
    auto samplesB = MeshPointSample::sample(b, sampOpts);

    MeshBVH bvhB;
    bvhB.build(b);
    MeshBVH bvhA;
    bvhA.build(a);

    float fwdSum = 0.f;
    float fwdMax = 0.f;
    if (bvhB.isValid()) {
        for (const auto& pt : samplesA.points) {
            float d = nearestDist(bvhB, pt);
            fwdSum += d;
            fwdMax = std::max(fwdMax, d);
        }
    }

    float bwdSum = 0.f;
    float bwdMax = 0.f;
    if (bvhA.isValid()) {
        for (const auto& pt : samplesB.points) {
            float d = nearestDist(bvhA, pt);
            bwdSum += d;
            bwdMax = std::max(bwdMax, d);
        }
    }

    size_t nA = samplesA.points.size();
    size_t nB = samplesB.points.size();

    result.forwardMax = fwdMax;
    result.backwardMax = bwdMax;
    result.symmetricMax = std::max(fwdMax, bwdMax);
    result.forwardMean = nA > 0 ? fwdSum / static_cast<float>(nA) : 0.f;
    result.backwardMean = nB > 0 ? bwdSum / static_cast<float>(nB) : 0.f;
    result.symmetricMean = std::max(result.forwardMean, result.backwardMean);

    return result;
}

} // namespace nexus::geometry
