#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Parametric — 2D sketch profile generator
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/parametric/ConstraintGraph.h>

namespace nexus::parametric {

struct SketchProfile {
    ConstraintGraph graph;
    ParametricEntityId plane = kInvalidEntityId;
    std::vector<ParametricEntityId> points;
};

class ParametricSketchFactory {
public:
    // Create an empty sketch on the XY plane at Z = 0.
    [[nodiscard]] static SketchProfile createSketch();

    // Add a point to the sketch, automatically constraining it to the XY plane.
    [[nodiscard]] static ParametricEntityId addSketchPoint(SketchProfile& sketch,
                                                            double x,
                                                            double y);

    // Create a line segment between two points in the sketch.
    // Returns a distance constraint id.
    [[nodiscard]] static ParametricConstraintId addLineSegment(SketchProfile& sketch,
                                                                ParametricEntityId p1,
                                                                ParametricEntityId p2);

    // Create a rectangle in the sketch. Returns the 4 corner point ids.
    // Order: origin, xHandle (origin + w), yHandle (origin + h), corner (origin + w + h).
    [[nodiscard]] static std::tuple<ParametricEntityId, ParametricEntityId,
                                     ParametricEntityId, ParametricEntityId>
    addRectangle(SketchProfile& sketch, double originX, double originY,
                 double width, double height);

    // Create a regular polygon approximating a circle in the sketch.
    // Returns the entity ids of the vertices.
    [[nodiscard]] static std::vector<ParametricEntityId>
    addCircle(SketchProfile& sketch, double centerX, double centerY,
              double radius, uint32_t segments = 32);

    // Create a polyline approximating a circular arc in the sketch.
    // Returns the entity ids of the vertices.
    [[nodiscard]] static std::vector<ParametricEntityId>
    addArc(SketchProfile& sketch, double centerX, double centerY,
           double radius, double startAngleDeg, double endAngleDeg,
           uint32_t segments = 16);

    // Create a polyline approximating an ellipse in the sketch.
    // Returns the entity ids of the vertices.
    [[nodiscard]] static std::vector<ParametricEntityId>
    addEllipse(SketchProfile& sketch, double centerX, double centerY,
               double radiusX, double radiusY, uint32_t segments = 32);

    // Create a polyline from a set of 2D points in the sketch.
    // Points are constrained to the sketch plane and connected with distance
    // constraints forming a closed or open chain.
    [[nodiscard]] static std::vector<ParametricEntityId>
    addPolyline(SketchProfile& sketch,
                const std::vector<std::pair<double, double>>& xyPoints,
                bool closed = true);
};

} // namespace nexus::parametric
