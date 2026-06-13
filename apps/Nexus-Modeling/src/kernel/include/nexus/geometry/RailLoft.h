#pragma once
// ── Nexus Geometry — RailLoft

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct RailLoftOptions {
    int32_t samplesPerRail = 64;
    bool closedProfile = false;
    int32_t profileSamples = 32;
};

class RailLoft {
public:
    using Vec3 = nexus::render::Vec3;

    struct ProfileLoop {
        std::vector<Vec3> points;
    };

    [[nodiscard]] static Mesh loft(const NurbsCurve& rail0,
                                    const NurbsCurve& rail1,
                                    const ProfileLoop& profile,
                                    const RailLoftOptions& opts = {});
};

} // namespace nexus::geometry
