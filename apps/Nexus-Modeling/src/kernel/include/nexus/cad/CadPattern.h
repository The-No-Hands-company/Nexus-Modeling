#pragma once
#include <nexus/cad/CadDocument.h>
#include <nexus/render/Camera.h>
#include <cstdint>
#include <vector>

namespace nexus::cad {
using Vec3 = nexus::render::Vec3;

struct LinearPattern { Vec3 direction{1,0,0}; uint32_t count=2; float spacing=10.f; };
struct CircularPattern { Vec3 center{}, axis{0,0,1}; uint32_t count=4; float angleDeg=360.f; };

class CadPattern {
public:
    [[nodiscard]] static std::vector<parametric::FeatureId> linear(
        CadDocument& doc, parametric::FeatureId sourceId, const LinearPattern& p) noexcept;
    [[nodiscard]] static std::vector<parametric::FeatureId> circular(
        CadDocument& doc, parametric::FeatureId sourceId, const CircularPattern& p) noexcept;
    [[nodiscard]] static parametric::FeatureId mirror(
        CadDocument& doc, parametric::FeatureId sourceId, const Vec3& planePoint, const Vec3& planeNormal) noexcept;
};
}
