#pragma once
#include <nexus/cad/CadDocument.h>
#include <nexus/render/Camera.h>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::cad {
using Vec3 = nexus::render::Vec3;
struct GDnTFrame { std::string geometricCharacteristic,toleranceStr="0.1",datumPrimary,datumSecondary,datumTertiary,modifier; };
struct PMIAnnotation { enum Type { Dimension,Note,GDnT,SurfaceFinish,WeldingSymbol,Datum,DatumTarget,Balloon }; Type type=Dimension; std::string text; Vec3 position{},direction{0,0,1}; parametric::FeatureId attachedFeature=parametric::kInvalidFeatureId; };
class CadMBD {
public: void addAnnotation(const PMIAnnotation&); void addGDnTFrame(const Vec3&,const GDnTFrame&,parametric::FeatureId); [[nodiscard]] static geometry::Mesh exportAnnotationsMesh(const std::vector<PMIAnnotation>&) noexcept; [[nodiscard]] const std::vector<PMIAnnotation>& annotations() const noexcept; void clear();
private: std::vector<PMIAnnotation> m_annot; };
}
