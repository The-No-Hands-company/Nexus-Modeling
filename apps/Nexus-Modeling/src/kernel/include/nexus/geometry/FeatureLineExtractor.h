#pragma once
// ── Nexus Geometry — FeatureLineExtractor

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct FeatureLineExtractorOptions {
    float dihedralAngleThresholdDegrees = 30.0f;
};

class FeatureLineExtractor {
public:
    using Vec3 = nexus::render::Vec3;

    struct Polyline {
        std::vector<Vec3> points;
        bool closed = false;
    };

    [[nodiscard]] static std::vector<Polyline> extract(
        const Mesh& surface,
        const FeatureLineExtractorOptions& opts = {});
};

} // namespace nexus::geometry
