#pragma once
#include <nexus/cad/CadDocument.h>
#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSketchProfile.h>

namespace nexus::cad {

struct AutoConstraintOptions {
    double snapRadius     = 0.5;
    double angleTolerance = 5.0;
    bool inferHorizontal = true, inferVertical = true, inferCoincident = true;
};

class CadAutoConstraintSketch {
public:
    explicit CadAutoConstraintSketch(CadDocument& doc);

    void beginSketch();
    void endSketch();

    [[nodiscard]] parametric::ParametricEntityId addPoint(double x, double y);
    [[nodiscard]] parametric::ParametricConstraintId addLine(parametric::ParametricEntityId p1, parametric::ParametricEntityId p2);
    [[nodiscard]] std::array<parametric::ParametricEntityId,4> addRectangle(double ox,double oy,double w,double h);
    [[nodiscard]] parametric::ParametricEntityId addCircle(double cx,double cy,double r);

    [[nodiscard]] parametric::SketchProfile& profile() noexcept { return m_profile; }
    void setOptions(const AutoConstraintOptions& opts) { m_opts = opts; }

private:
    void inferConstraints(parametric::ParametricEntityId newPoint);
    CadDocument& m_doc;
    parametric::SketchProfile m_profile;
    AutoConstraintOptions m_opts;
    parametric::FeatureId m_sketchFeatureId = parametric::kInvalidFeatureId;
};

}
