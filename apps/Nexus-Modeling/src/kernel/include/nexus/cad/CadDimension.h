#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Dimension System
//
//  Linear, radial, angular, and ordinate dimensions with tolerance support.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/render/Camera.h>

#include <string>
#include <vector>

namespace nexus::cad {

using Vec3 = nexus::render::Vec3;

enum class DimensionType : uint8_t {
    Linear,
    Aligned,
    Radial,
    Diameter,
    Angular,
    Ordinate,
};

struct DimensionTolerance {
    double upper = 0.0;
    double lower = 0.0;
    bool   symmetric = true;
};

struct CadDimension {
    DimensionType type = DimensionType::Linear;
    Vec3  point1{};
    Vec3  point2{};
    Vec3  textPosition{};
    double value = 0.0;
    DimensionTolerance tolerance;
    std::string prefix;
    std::string suffix;
    parametric::FeatureId featureId = parametric::kInvalidFeatureId;
};

class CadDimensionManager {
public:
    // Add a linear dimension between two points.
    [[nodiscard]] uint32_t addLinear(const Vec3& p1, const Vec3& p2,
                                       const Vec3& textPos);

    // Add a radial dimension to a circle/arc.
    [[nodiscard]] uint32_t addRadial(const Vec3& center, double radius,
                                       const Vec3& textPos);

    // Add an angular dimension between two lines.
    [[nodiscard]] uint32_t addAngular(const Vec3& vertex,
                                        const Vec3& dir1, const Vec3& dir2,
                                        const Vec3& textPos);

    [[nodiscard]] const std::vector<CadDimension>& dimensions() const noexcept;
    void clear();

private:
    std::vector<CadDimension> m_dims;
};

} // namespace nexus::cad
