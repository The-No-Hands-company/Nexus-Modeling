#pragma once
// ── Nexus Geometry — PointCloudNormals

#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct PointCloudNormalsOptions {
    int32_t kNeighbors = 16;
    Vec3 viewpoint = {0.f, 0.f, 0.f};
    bool orientToViewpoint = true;
};

class PointCloudNormals {
public:
    struct Result {
        std::vector<Vec3> normals;
    };

    [[nodiscard]] static Result estimate(const std::vector<Vec3>& points,
                                         const PointCloudNormalsOptions& opts = {});
};

} // namespace nexus::geometry
