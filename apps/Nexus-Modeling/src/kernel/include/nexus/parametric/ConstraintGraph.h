#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nexus::parametric {

using ParametricEntityId = uint64_t;
using ParametricConstraintId = uint64_t;

inline constexpr ParametricEntityId kInvalidEntityId = 0;
inline constexpr ParametricConstraintId kInvalidConstraintId = 0;

struct ParametricPoint3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct ParametricEntity {
    ParametricEntityId id = kInvalidEntityId;
    ParametricPoint3 point{};
};

enum class ConstraintType : uint8_t {
    Distance,
    Coincident,
    AxisAlignedDistance,
    Angle,
    EqualDistance,
    PointOnLine,
};

enum class Axis : uint8_t {
    X,
    Y,
    Z,
};

struct DistanceConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityA = kInvalidEntityId;
    ParametricEntityId entityB = kInvalidEntityId;
    double targetDistance = 0.0;
};

struct CoincidentConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityA = kInvalidEntityId;
    ParametricEntityId entityB = kInvalidEntityId;
};

struct AxisAlignedDistanceConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityA = kInvalidEntityId;
    ParametricEntityId entityB = kInvalidEntityId;
    Axis axis = Axis::X;
    double targetDistance = 0.0;
};

struct AngleConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityA = kInvalidEntityId;
    ParametricEntityId entityB = kInvalidEntityId;
    ParametricEntityId entityC = kInvalidEntityId;
    double targetAngleDegrees = 0.0;
};

struct EqualDistanceConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityA = kInvalidEntityId;
    ParametricEntityId entityB = kInvalidEntityId;
    ParametricEntityId entityC = kInvalidEntityId;
    ParametricEntityId entityD = kInvalidEntityId;
};

struct PointOnLineConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityP = kInvalidEntityId;
    ParametricEntityId entityL0 = kInvalidEntityId;
    ParametricEntityId entityL1 = kInvalidEntityId;
};

struct SketchPlaneConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityPlane = kInvalidEntityId;
    ParametricEntityId entityPoint = kInvalidEntityId;
};

struct DegreesOfFreedom {
    int32_t freeVariables = 0;
    int32_t effectiveConstraints = 0;
    int32_t estimatedRemainingDOF = 0;
    bool likelyOverconstrained = false;
};

class ConstraintGraph {
public:
    [[nodiscard]] ParametricEntityId addPoint(const ParametricPoint3& point) noexcept;
    [[nodiscard]] bool removeEntity(ParametricEntityId id) noexcept;
    [[nodiscard]] bool hasEntity(ParametricEntityId id) const noexcept;

    [[nodiscard]] bool setPoint(ParametricEntityId id, const ParametricPoint3& point) noexcept;
    [[nodiscard]] const ParametricPoint3* point(ParametricEntityId id) const noexcept;

    [[nodiscard]] ParametricConstraintId addDistanceConstraint(ParametricEntityId entityA,
                                                               ParametricEntityId entityB,
                                                               double targetDistance) noexcept;
    [[nodiscard]] ParametricConstraintId addCoincidentConstraint(ParametricEntityId entityA,
                                                                 ParametricEntityId entityB) noexcept;
    [[nodiscard]] ParametricConstraintId addAxisAlignedDistanceConstraint(ParametricEntityId entityA,
                                                                          ParametricEntityId entityB,
                                                                          Axis axis,
                                                                          double targetDistance) noexcept;
    [[nodiscard]] ParametricConstraintId addAngleConstraint(ParametricEntityId entityA,
                                                            ParametricEntityId entityB,
                                                            ParametricEntityId entityC,
                                                            double targetAngleDegrees) noexcept;
    [[nodiscard]] ParametricConstraintId addEqualDistanceConstraint(ParametricEntityId entityA,
                                                                    ParametricEntityId entityB,
                                                                    ParametricEntityId entityC,
                                                                    ParametricEntityId entityD) noexcept;
    [[nodiscard]] ParametricConstraintId addPointOnLineConstraint(ParametricEntityId entityP,
                                                                  ParametricEntityId entityL0,
                                                                  ParametricEntityId entityL1) noexcept;
    [[nodiscard]] ParametricConstraintId addSketchPlaneConstraint(ParametricEntityId entityPlane,
                                                                   ParametricEntityId entityPoint) noexcept;

    // Constraint combinators — compose higher-level constraints from primitives.
    [[nodiscard]] ParametricConstraintId addParallelConstraint(ParametricEntityId a0,ParametricEntityId a1,
        ParametricEntityId b0,ParametricEntityId b1) noexcept;
    [[nodiscard]] ParametricConstraintId addPerpendicularConstraint(ParametricEntityId a0,ParametricEntityId a1,
        ParametricEntityId b0,ParametricEntityId b1) noexcept;
    [[nodiscard]] ParametricConstraintId addCollinearConstraint(ParametricEntityId a0,ParametricEntityId a1,
        ParametricEntityId b0,ParametricEntityId b1) noexcept;
    [[nodiscard]] ParametricConstraintId addHorizontalConstraint(ParametricEntityId a,ParametricEntityId b) noexcept;
    [[nodiscard]] ParametricConstraintId addVerticalConstraint(ParametricEntityId a,ParametricEntityId b) noexcept;
    [[nodiscard]] ParametricConstraintId addConcentricConstraint(ParametricEntityId ca,ParametricEntityId cb) noexcept;
    [[nodiscard]] ParametricConstraintId addMidpointConstraint(ParametricEntityId p,ParametricEntityId a,
        ParametricEntityId b) noexcept;
    [[nodiscard]] ParametricConstraintId addSymmetricConstraint(ParametricEntityId a,ParametricEntityId b,
        ParametricEntityId l0,ParametricEntityId l1) noexcept;
    [[nodiscard]] ParametricConstraintId addMateConstraint(ParametricEntityId fa0,ParametricEntityId fa1,
        ParametricEntityId fa2,ParametricEntityId fb0,ParametricEntityId fb1,ParametricEntityId fb2) noexcept;
    [[nodiscard]] ParametricConstraintId addAlignConstraint(ParametricEntityId axA0,ParametricEntityId axA1,
        ParametricEntityId axB0,ParametricEntityId axB1) noexcept;
    [[nodiscard]] ParametricConstraintId addGearConstraint(ParametricEntityId ca,ParametricEntityId cb,
        double rA,double rB) noexcept;

    [[nodiscard]] bool removeConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] bool hasConstraint(ParametricConstraintId id) const noexcept;

    [[nodiscard]] const std::vector<ParametricEntity>& entities() const noexcept { return m_entities; }
    [[nodiscard]] const std::vector<DistanceConstraint>& distanceConstraints() const noexcept { return m_distanceConstraints; }
    [[nodiscard]] const std::vector<CoincidentConstraint>& coincidentConstraints() const noexcept { return m_coincidentConstraints; }
    [[nodiscard]] const std::vector<AxisAlignedDistanceConstraint>& axisAlignedDistanceConstraints() const noexcept { return m_axisAlignedDistanceConstraints; }
    [[nodiscard]] const std::vector<AngleConstraint>& angleConstraints() const noexcept { return m_angleConstraints; }
    [[nodiscard]] const std::vector<EqualDistanceConstraint>& equalDistanceConstraints() const noexcept { return m_equalDistanceConstraints; }
    [[nodiscard]] const std::vector<PointOnLineConstraint>& pointOnLineConstraints() const noexcept { return m_pointOnLineConstraints; }
    [[nodiscard]] const std::vector<SketchPlaneConstraint>& sketchPlaneConstraints() const noexcept { return m_sketchPlaneConstraints; }

    [[nodiscard]] size_t entityCount() const noexcept { return m_entities.size(); }
    [[nodiscard]] size_t distanceConstraintCount() const noexcept { return m_distanceConstraints.size(); }
    [[nodiscard]] size_t coincidentConstraintCount() const noexcept { return m_coincidentConstraints.size(); }
    [[nodiscard]] size_t axisAlignedDistanceConstraintCount() const noexcept { return m_axisAlignedDistanceConstraints.size(); }
    [[nodiscard]] size_t angleConstraintCount() const noexcept { return m_angleConstraints.size(); }
    [[nodiscard]] size_t equalDistanceConstraintCount() const noexcept { return m_equalDistanceConstraints.size(); }
    [[nodiscard]] size_t pointOnLineConstraintCount() const noexcept { return m_pointOnLineConstraints.size(); }
    [[nodiscard]] size_t sketchPlaneConstraintCount() const noexcept { return m_sketchPlaneConstraints.size(); }

    [[nodiscard]] size_t totalConstraintCount() const noexcept;

    [[nodiscard]] DegreesOfFreedom analyzeDegreesOfFreedom() const noexcept;

private:
    [[nodiscard]] std::vector<ParametricEntity>::iterator findEntity(ParametricEntityId id) noexcept;
    [[nodiscard]] std::vector<ParametricEntity>::const_iterator findEntity(ParametricEntityId id) const noexcept;
    [[nodiscard]] std::vector<DistanceConstraint>::iterator findConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<DistanceConstraint>::const_iterator findConstraint(ParametricConstraintId id) const noexcept;
    [[nodiscard]] std::vector<CoincidentConstraint>::iterator findCoincidentConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<CoincidentConstraint>::const_iterator findCoincidentConstraint(ParametricConstraintId id) const noexcept;
    [[nodiscard]] std::vector<AxisAlignedDistanceConstraint>::iterator findAxisAlignedDistanceConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<AxisAlignedDistanceConstraint>::const_iterator findAxisAlignedDistanceConstraint(ParametricConstraintId id) const noexcept;
    [[nodiscard]] std::vector<AngleConstraint>::iterator findAngleConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<AngleConstraint>::const_iterator findAngleConstraint(ParametricConstraintId id) const noexcept;
    [[nodiscard]] std::vector<EqualDistanceConstraint>::iterator findEqualDistanceConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<EqualDistanceConstraint>::const_iterator findEqualDistanceConstraint(ParametricConstraintId id) const noexcept;
    [[nodiscard]] std::vector<PointOnLineConstraint>::iterator findPointOnLineConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<PointOnLineConstraint>::const_iterator findPointOnLineConstraint(ParametricConstraintId id) const noexcept;
    [[nodiscard]] std::vector<SketchPlaneConstraint>::iterator findSketchPlaneConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<SketchPlaneConstraint>::const_iterator findSketchPlaneConstraint(ParametricConstraintId id) const noexcept;

    std::vector<ParametricEntity> m_entities;
    std::vector<DistanceConstraint> m_distanceConstraints;
    std::vector<CoincidentConstraint> m_coincidentConstraints;
    std::vector<AxisAlignedDistanceConstraint> m_axisAlignedDistanceConstraints;
    std::vector<AngleConstraint> m_angleConstraints;
    std::vector<EqualDistanceConstraint> m_equalDistanceConstraints;
    std::vector<PointOnLineConstraint> m_pointOnLineConstraints;
    std::vector<SketchPlaneConstraint> m_sketchPlaneConstraints;

    ParametricEntityId m_nextEntityId = 1;
    ParametricConstraintId m_nextConstraintId = 1;
};

} // namespace nexus::parametric
