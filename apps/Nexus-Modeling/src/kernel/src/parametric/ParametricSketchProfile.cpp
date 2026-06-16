#include <nexus/parametric/ParametricSketchProfile.h>

#include <cmath>
#include <numbers>

namespace nexus::parametric {

SketchProfile ParametricSketchFactory::createSketch()
{
    SketchProfile sketch;
    sketch.plane = sketch.graph.addPoint({0.0, 0.0, 0.0});
    return sketch;
}

ParametricEntityId ParametricSketchFactory::addSketchPoint(SketchProfile& sketch,
                                                            double x,
                                                            double y)
{
    const ParametricEntityId id = sketch.graph.addPoint({x, y, 0.0});
    if (id != kInvalidEntityId && sketch.plane != kInvalidEntityId) {
        (void)sketch.graph.addSketchPlaneConstraint(sketch.plane, id);
    }
    if (id != kInvalidEntityId) {
        sketch.points.push_back(id);
    }
    return id;
}

ParametricConstraintId ParametricSketchFactory::addLineSegment(SketchProfile& sketch,
                                                                ParametricEntityId p1,
                                                                ParametricEntityId p2)
{
    if (p1 == kInvalidEntityId || p2 == kInvalidEntityId ||
        !sketch.graph.hasEntity(p1) || !sketch.graph.hasEntity(p2)) {
        return kInvalidConstraintId;
    }

    const double dx = sketch.graph.point(p2)->x - sketch.graph.point(p1)->x;
    const double dy = sketch.graph.point(p2)->y - sketch.graph.point(p1)->y;
    const double length = std::sqrt(dx * dx + dy * dy);

    return sketch.graph.addDistanceConstraint(p1, p2, length);
}

std::tuple<ParametricEntityId, ParametricEntityId,
           ParametricEntityId, ParametricEntityId>
ParametricSketchFactory::addRectangle(SketchProfile& sketch,
                                       double originX, double originY,
                                       double width, double height)
{
    if (width <= 0.0 || height <= 0.0) {
        return {kInvalidEntityId, kInvalidEntityId, kInvalidEntityId, kInvalidEntityId};
    }

    const ParametricEntityId origin = addSketchPoint(sketch, originX, originY);
    const ParametricEntityId xHandle = addSketchPoint(sketch, originX + width, originY);
    const ParametricEntityId yHandle = addSketchPoint(sketch, originX, originY + height);
    const ParametricEntityId corner = addSketchPoint(sketch, originX + width, originY + height);

    // Use axis-aligned distance constraints to form a proper rectangle.
    (void)sketch.graph.addAxisAlignedDistanceConstraint(origin, xHandle, Axis::X, width);
    (void)sketch.graph.addAxisAlignedDistanceConstraint(origin, yHandle, Axis::Y, height);
    (void)sketch.graph.addAxisAlignedDistanceConstraint(xHandle, corner, Axis::Y, height);
    (void)sketch.graph.addAxisAlignedDistanceConstraint(yHandle, corner, Axis::X, width);

    return {origin, xHandle, yHandle, corner};
}

std::vector<ParametricEntityId>
ParametricSketchFactory::addCircle(SketchProfile& sketch,
                                    double centerX, double centerY,
                                    double radius, uint32_t segments)
{
    if (segments < 3) segments = 3;
    if (radius <= 0.0) return {};

    std::vector<ParametricEntityId> ids;
    ids.reserve(segments);

    for (uint32_t i = 0; i < segments; ++i) {
        const double angle = 2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(segments);
        const double x = centerX + radius * std::cos(angle);
        const double y = centerY + radius * std::sin(angle);
        ids.push_back(addSketchPoint(sketch, x, y));
    }

    // Connect the vertices with distance constraints.
    for (uint32_t i = 0; i < segments; ++i) {
        const uint32_t j = (i + 1) % segments;
        if (ids[i] != kInvalidEntityId && ids[j] != kInvalidEntityId) {
            const double dx = sketch.graph.point(ids[j])->x - sketch.graph.point(ids[i])->x;
            const double dy = sketch.graph.point(ids[j])->y - sketch.graph.point(ids[i])->y;
            const double length = std::sqrt(dx * dx + dy * dy);
            (void)sketch.graph.addDistanceConstraint(ids[i], ids[j], length);
        }
    }

    return ids;
}

std::vector<ParametricEntityId>
ParametricSketchFactory::addArc(SketchProfile& sketch,
                                 double centerX, double centerY,
                                 double radius, double startAngleDeg, double endAngleDeg,
                                 uint32_t segments)
{
    if (segments < 2) segments = 2;
    if (radius <= 0.0) return {};
    if (startAngleDeg >= endAngleDeg) return {};

    const double startRad = startAngleDeg * std::numbers::pi / 180.0;
    const double endRad   = endAngleDeg * std::numbers::pi / 180.0;
    const double range    = endRad - startRad;

    std::vector<ParametricEntityId> ids;
    ids.reserve(segments + 1);

    for (uint32_t i = 0; i <= segments; ++i) {
        const double angle = startRad + range * static_cast<double>(i) / static_cast<double>(segments);
        const double x = centerX + radius * std::cos(angle);
        const double y = centerY + radius * std::sin(angle);
        ids.push_back(addSketchPoint(sketch, x, y));
    }

    for (uint32_t i = 0; i < segments; ++i) {
        if (ids[i] != kInvalidEntityId && ids[i + 1] != kInvalidEntityId) {
            const double dx = sketch.graph.point(ids[i + 1])->x - sketch.graph.point(ids[i])->x;
            const double dy = sketch.graph.point(ids[i + 1])->y - sketch.graph.point(ids[i])->y;
            const double length = std::sqrt(dx * dx + dy * dy);
            (void)sketch.graph.addDistanceConstraint(ids[i], ids[i + 1], length);
        }
    }

    return ids;
}

std::vector<ParametricEntityId>
ParametricSketchFactory::addEllipse(SketchProfile& sketch,
                                     double centerX, double centerY,
                                     double radiusX, double radiusY, uint32_t segments)
{
    if (segments < 3) segments = 3;
    if (radiusX <= 0.0 || radiusY <= 0.0) return {};

    std::vector<ParametricEntityId> ids;
    ids.reserve(segments);

    for (uint32_t i = 0; i < segments; ++i) {
        const double angle = 2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(segments);
        const double x = centerX + radiusX * std::cos(angle);
        const double y = centerY + radiusY * std::sin(angle);
        ids.push_back(addSketchPoint(sketch, x, y));
    }

    for (uint32_t i = 0; i < segments; ++i) {
        const uint32_t j = (i + 1) % segments;
        if (ids[i] != kInvalidEntityId && ids[j] != kInvalidEntityId) {
            const double dx = sketch.graph.point(ids[j])->x - sketch.graph.point(ids[i])->x;
            const double dy = sketch.graph.point(ids[j])->y - sketch.graph.point(ids[i])->y;
            const double length = std::sqrt(dx * dx + dy * dy);
            (void)sketch.graph.addDistanceConstraint(ids[i], ids[j], length);
        }
    }

    return ids;
}

std::vector<ParametricEntityId>
ParametricSketchFactory::addPolyline(SketchProfile& sketch,
                                      const std::vector<std::pair<double, double>>& xyPoints,
                                      bool closed)
{
    if (xyPoints.size() < 2) return {};

    std::vector<ParametricEntityId> ids;
    ids.reserve(xyPoints.size());

    for (const auto& [x, y] : xyPoints) {
        ids.push_back(addSketchPoint(sketch, x, y));
    }

    const size_t n = ids.size();
    const size_t edgeCount = closed ? n : n - 1;

    for (size_t i = 0; i < edgeCount; ++i) {
        const size_t j = (i + 1) % n;
        if (ids[i] != kInvalidEntityId && ids[j] != kInvalidEntityId) {
            const double dx = sketch.graph.point(ids[j])->x - sketch.graph.point(ids[i])->x;
            const double dy = sketch.graph.point(ids[j])->y - sketch.graph.point(ids[i])->y;
            const double length = std::sqrt(dx * dx + dy * dy);
            (void)sketch.graph.addDistanceConstraint(ids[i], ids[j], length);
        }
    }

    return ids;
}

} // namespace nexus::parametric
