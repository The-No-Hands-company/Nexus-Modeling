#pragma once
#include <nexus/cad/CadDocument.h>
#include <nexus/render/Camera.h>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::cad {
using Vec3 = nexus::render::Vec3;

enum class HoleType : uint8_t { Simple, Counterbore, Countersink, Tapped };
struct HoleSpec { HoleType type=HoleType::Simple; double diameter=6,depth=20,counterboreDiameter=12,counterboreDepth=5,countersinkAngle=82; std::string threadSize; Vec3 position{},direction{0,0,-1}; };

class CadHoleWizard {
public:
    [[nodiscard]] static parametric::FeatureId create(CadDocument& doc, const HoleSpec& spec) noexcept;
    [[nodiscard]] static std::vector<double> standardMetricClearanceHoles() noexcept;
    [[nodiscard]] static std::vector<std::string> standardThreadSizes() noexcept;
};
}
