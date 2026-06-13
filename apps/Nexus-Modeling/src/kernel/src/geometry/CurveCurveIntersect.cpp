#include <nexus/geometry/CurveCurveIntersect.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

std::vector<CurveCurveIntersect::Intersection> CurveCurveIntersect::intersect(
    const NurbsCurve& a,
    const NurbsCurve& b,
    int32_t seedSamples,
    float tolerance) {

    std::vector<Intersection> result;
    if (!a.isValid() || !b.isValid() || seedSamples < 2) return result;

    auto domA = a.domain();
    auto domB = b.domain();

    int32_t coarse = std::max(seedSamples, 4);

    struct Seed {
        float s, t;
        float dist;
    };

    std::vector<Seed> seeds;
    for (int32_t i = 0; i < coarse; ++i) {
        float s = domA.first + (domA.second - domA.first) * static_cast<float>(i) / static_cast<float>(coarse - 1);
        for (int32_t j = 0; j < coarse; ++j) {
            float t = domB.first + (domB.second - domB.first) * static_cast<float>(j) / static_cast<float>(coarse - 1);
            Vec3 pa = a.evaluate(s);
            Vec3 pb = b.evaluate(t);
            float d = (pa - pb).lengthSq();
            seeds.push_back({s, t, d});
        }
    }

    std::sort(seeds.begin(), seeds.end(),
              [](const Seed& x, const Seed& y) { return x.dist < y.dist; });

    std::vector<Seed> candidates;
    float seedThreshold = std::min(seeds.front().dist * 10.0f,
                                   seeds.front().dist + 0.1f);
    seedThreshold = std::max(seedThreshold, 0.001f);

    for (const auto& seed : seeds) {
        if (seed.dist > seedThreshold) break;
        candidates.push_back(seed);
    }

    if (candidates.size() > 20) candidates.resize(20);

    for (auto& cand : candidates) {
        float s = cand.s;
        float t = cand.t;

        for (int32_t iter = 0; iter < 32; ++iter) {
            Vec3 pa = a.evaluate(s);
            Vec3 pb = b.evaluate(t);
            Vec3 dap = a.derivative(s);
            Vec3 dbp = b.derivative(t);

            Vec3 diff = pa - pb;

            float E = dap.dot(dap);
            float F = -dap.dot(dbp);
            float G = dbp.dot(dbp);

            float R = diff.dot(dap);
            float S = -diff.dot(dbp);

            float det = E * G - F * F;
            if (std::abs(det) < 1e-12f) break;

            float ds = (G * R - F * S) / det;
            float dt = (E * S - F * R) / det;

            float damping = 0.6f;
            s -= ds * damping;
            t += dt * damping;

            s = std::clamp(s, domA.first, domA.second);
            t = std::clamp(t, domB.first, domB.second);

            if (std::abs(ds) < tolerance && std::abs(dt) < tolerance) break;
        }

        Vec3 pa = a.evaluate(s);
        Vec3 pb = b.evaluate(t);
        float dist = (pa - pb).length();

        if (dist < tolerance * 100.f) {
            bool duplicate = false;
            for (const auto& existing : result) {
                if (std::abs(existing.s - s) < tolerance &&
                    std::abs(existing.t - t) < tolerance) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                result.push_back({s, t, Vec3{(pa.x+pb.x)*0.5f, (pa.y+pb.y)*0.5f, (pa.z+pb.z)*0.5f}, dist});
            }
        }
    }

    std::sort(result.begin(), result.end(),
              [](const Intersection& x, const Intersection& y) {
                  return x.distance < y.distance;
              });

    return result;
}

} // namespace nexus::geometry
