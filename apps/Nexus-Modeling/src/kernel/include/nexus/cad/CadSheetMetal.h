#pragma once
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadExpression.h>
#include <nexus/geometry/Mesh.h>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::cad {
class CadEquationSketch { public: void defineGlobal(const std::string& n, double v); bool driveDimension(const std::string& n, const std::string& e); void update(CadDocument& doc); [[nodiscard]] const CadExpressionEngine& engine() const noexcept; private: CadExpressionEngine m_engine; std::vector<std::tuple<std::string,std::string,parametric::FeatureId>> m_driven; };
struct SheetMetalParams { double thickness=1.5,bendRadius=2.0,kFactor=0.44; };
class CadSheetMetal { public: [[nodiscard]] static parametric::FeatureId createBaseFlange(CadDocument&,parametric::FeatureId,const SheetMetalParams&) noexcept; [[nodiscard]] static parametric::FeatureId addEdgeFlange(CadDocument&,parametric::FeatureId,uint32_t,double,double=90) noexcept; [[nodiscard]] static geometry::Mesh unfold(const CadDocument&,parametric::FeatureId) noexcept; [[nodiscard]] static std::vector<SheetMetalParams> standardGauges() noexcept; };
}
