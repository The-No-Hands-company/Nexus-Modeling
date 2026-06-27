#include <nexus/parametric/ConstraintGraph.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace nexus::parametric {

namespace {

bool isFiniteDouble(double v) noexcept
{
    return (std::bit_cast<uint64_t>(v) & 0x7FF0000000000000ULL) != 0x7FF0000000000000ULL;
}

bool isFinitePoint(const ParametricPoint3& point) noexcept
{
    return isFiniteDouble(point.x)
        && isFiniteDouble(point.y)
        && isFiniteDouble(point.z);
}

bool isFiniteAngle(double degrees) noexcept
{
    return isFiniteDouble(degrees) && degrees >= -360.0 && degrees <= 360.0;
}

} // namespace

ParametricEntityId ConstraintGraph::addPoint(const ParametricPoint3& point) noexcept
{
    if (!isFinitePoint(point)) {
        return kInvalidEntityId;
    }

    const ParametricEntityId id = m_nextEntityId++;
    m_entities.push_back(ParametricEntity{id, point});
    return id;
}

bool ConstraintGraph::removeEntity(ParametricEntityId id) noexcept
{
    const auto it = findEntity(id);
    if (it == m_entities.end()) {
        return false;
    }

    m_entities.erase(it);

    m_distanceConstraints.erase(
        std::remove_if(m_distanceConstraints.begin(),
                       m_distanceConstraints.end(),
                       [id](const DistanceConstraint& c) {
                           return c.entityA == id || c.entityB == id;
                       }),
        m_distanceConstraints.end());

    m_coincidentConstraints.erase(
        std::remove_if(m_coincidentConstraints.begin(),
                       m_coincidentConstraints.end(),
                       [id](const CoincidentConstraint& c) {
                           return c.entityA == id || c.entityB == id;
                       }),
        m_coincidentConstraints.end());

    m_axisAlignedDistanceConstraints.erase(
        std::remove_if(m_axisAlignedDistanceConstraints.begin(),
                       m_axisAlignedDistanceConstraints.end(),
                       [id](const AxisAlignedDistanceConstraint& c) {
                           return c.entityA == id || c.entityB == id;
                       }),
        m_axisAlignedDistanceConstraints.end());

    m_angleConstraints.erase(
        std::remove_if(m_angleConstraints.begin(),
                       m_angleConstraints.end(),
                       [id](const AngleConstraint& c) {
                           return c.entityA == id || c.entityB == id || c.entityC == id;
                       }),
        m_angleConstraints.end());

    m_equalDistanceConstraints.erase(
        std::remove_if(m_equalDistanceConstraints.begin(),
                       m_equalDistanceConstraints.end(),
                       [id](const EqualDistanceConstraint& c) {
                           return c.entityA == id || c.entityB == id ||
                                  c.entityC == id || c.entityD == id;
                       }),
        m_equalDistanceConstraints.end());

    m_pointOnLineConstraints.erase(
        std::remove_if(m_pointOnLineConstraints.begin(),
                       m_pointOnLineConstraints.end(),
                       [id](const PointOnLineConstraint& c) {
                           return c.entityP == id || c.entityL0 == id || c.entityL1 == id;
                       }),
        m_pointOnLineConstraints.end());

    m_sketchPlaneConstraints.erase(
        std::remove_if(m_sketchPlaneConstraints.begin(),
                       m_sketchPlaneConstraints.end(),
                       [id](const SketchPlaneConstraint& c) {
                           return c.entityPlane == id || c.entityPoint == id;
                       }),
        m_sketchPlaneConstraints.end());

    return true;
}

bool ConstraintGraph::hasEntity(ParametricEntityId id) const noexcept
{
    return findEntity(id) != m_entities.end();
}

bool ConstraintGraph::setPoint(ParametricEntityId id, const ParametricPoint3& point) noexcept
{
    const auto it = findEntity(id);
    if (it == m_entities.end() || !isFinitePoint(point)) {
        return false;
    }

    it->point = point;
    return true;
}

const ParametricPoint3* ConstraintGraph::point(ParametricEntityId id) const noexcept
{
    const auto it = findEntity(id);
    if (it == m_entities.end()) {
        return nullptr;
    }
    return &it->point;
}

ParametricConstraintId ConstraintGraph::addDistanceConstraint(ParametricEntityId entityA,
                                                               ParametricEntityId entityB,
                                                               double targetDistance) noexcept
{
    if (entityA == entityB || entityA == kInvalidEntityId || entityB == kInvalidEntityId ||
        !isFiniteDouble(targetDistance) || targetDistance < 0.0 ||
        !hasEntity(entityA) || !hasEntity(entityB)) {
        return kInvalidConstraintId;
    }

    const ParametricConstraintId id = m_nextConstraintId++;
    m_distanceConstraints.push_back(DistanceConstraint{id, entityA, entityB, targetDistance});
    return id;
}

ParametricConstraintId ConstraintGraph::addCoincidentConstraint(ParametricEntityId entityA,
                                                                 ParametricEntityId entityB) noexcept
{
    if (entityA == entityB || entityA == kInvalidEntityId || entityB == kInvalidEntityId ||
        !hasEntity(entityA) || !hasEntity(entityB)) {
        return kInvalidConstraintId;
    }

    const ParametricConstraintId id = m_nextConstraintId++;
    m_coincidentConstraints.push_back(CoincidentConstraint{id, entityA, entityB});
    return id;
}

ParametricConstraintId ConstraintGraph::addAxisAlignedDistanceConstraint(ParametricEntityId entityA,
                                                                          ParametricEntityId entityB,
                                                                          Axis axis,
                                                                          double targetDistance) noexcept
{
    if (entityA == entityB || entityA == kInvalidEntityId || entityB == kInvalidEntityId ||
        !isFiniteDouble(targetDistance) || targetDistance < 0.0 ||
        !hasEntity(entityA) || !hasEntity(entityB)) {
        return kInvalidConstraintId;
    }

    const ParametricConstraintId id = m_nextConstraintId++;
    m_axisAlignedDistanceConstraints.push_back(
        AxisAlignedDistanceConstraint{id, entityA, entityB, axis, targetDistance});
    return id;
}

ParametricConstraintId ConstraintGraph::addAngleConstraint(ParametricEntityId entityA,
                                                            ParametricEntityId entityB,
                                                            ParametricEntityId entityC,
                                                            double targetAngleDegrees) noexcept
{
    if (entityA == entityB || entityB == entityC || entityA == entityC ||
        entityA == kInvalidEntityId || entityB == kInvalidEntityId || entityC == kInvalidEntityId ||
        !isFiniteAngle(targetAngleDegrees) ||
        !hasEntity(entityA) || !hasEntity(entityB) || !hasEntity(entityC)) {
        return kInvalidConstraintId;
    }

    const ParametricConstraintId id = m_nextConstraintId++;
    m_angleConstraints.push_back(AngleConstraint{id, entityA, entityB, entityC, targetAngleDegrees});
    return id;
}

ParametricConstraintId ConstraintGraph::addEqualDistanceConstraint(ParametricEntityId entityA,
                                                                    ParametricEntityId entityB,
                                                                    ParametricEntityId entityC,
                                                                    ParametricEntityId entityD) noexcept
{
    if (entityA == entityB || entityC == entityD ||
        entityA == kInvalidEntityId || entityB == kInvalidEntityId ||
        entityC == kInvalidEntityId || entityD == kInvalidEntityId ||
        !hasEntity(entityA) || !hasEntity(entityB) ||
        !hasEntity(entityC) || !hasEntity(entityD)) {
        return kInvalidConstraintId;
    }

    const ParametricConstraintId id = m_nextConstraintId++;
    m_equalDistanceConstraints.push_back(EqualDistanceConstraint{id, entityA, entityB, entityC, entityD});
    return id;
}

ParametricConstraintId ConstraintGraph::addPointOnLineConstraint(ParametricEntityId entityP,
                                                                  ParametricEntityId entityL0,
                                                                  ParametricEntityId entityL1) noexcept
{
    if (entityP == entityL0 || entityP == entityL1 || entityL0 == entityL1 ||
        entityP == kInvalidEntityId || entityL0 == kInvalidEntityId || entityL1 == kInvalidEntityId ||
        !hasEntity(entityP) || !hasEntity(entityL0) || !hasEntity(entityL1)) {
        return kInvalidConstraintId;
    }

    const ParametricConstraintId id = m_nextConstraintId++;
    m_pointOnLineConstraints.push_back(PointOnLineConstraint{id, entityP, entityL0, entityL1});
    return id;
}

ParametricConstraintId ConstraintGraph::addSketchPlaneConstraint(ParametricEntityId entityPlane,
                                                                  ParametricEntityId entityPoint) noexcept
{
    if (entityPlane == entityPoint || entityPlane == kInvalidEntityId || entityPoint == kInvalidEntityId ||
        !hasEntity(entityPlane) || !hasEntity(entityPoint)) {
        return kInvalidConstraintId;
    }

    const ParametricConstraintId id = m_nextConstraintId++;
    m_sketchPlaneConstraints.push_back(SketchPlaneConstraint{id, entityPlane, entityPoint});
    return id;
}

bool ConstraintGraph::removeConstraint(ParametricConstraintId id) noexcept
{
    {
        auto it = findConstraint(id);
        if (it != m_distanceConstraints.end()) {
            m_distanceConstraints.erase(it);
            return true;
        }
    }

    {
        auto it = findCoincidentConstraint(id);
        if (it != m_coincidentConstraints.end()) {
            m_coincidentConstraints.erase(it);
            return true;
        }
    }

    {
        auto it = findAxisAlignedDistanceConstraint(id);
        if (it != m_axisAlignedDistanceConstraints.end()) {
            m_axisAlignedDistanceConstraints.erase(it);
            return true;
        }
    }

    {
        auto it = findAngleConstraint(id);
        if (it != m_angleConstraints.end()) {
            m_angleConstraints.erase(it);
            return true;
        }
    }

    {
        auto it = findEqualDistanceConstraint(id);
        if (it != m_equalDistanceConstraints.end()) {
            m_equalDistanceConstraints.erase(it);
            return true;
        }
    }

    {
        auto it = findPointOnLineConstraint(id);
        if (it != m_pointOnLineConstraints.end()) {
            m_pointOnLineConstraints.erase(it);
            return true;
        }
    }

    {
        auto it = findSketchPlaneConstraint(id);
        if (it != m_sketchPlaneConstraints.end()) {
            m_sketchPlaneConstraints.erase(it);
            return true;
        }
    }

    return false;
}

bool ConstraintGraph::hasConstraint(ParametricConstraintId id) const noexcept
{
    return findConstraint(id) != m_distanceConstraints.end() ||
           findCoincidentConstraint(id) != m_coincidentConstraints.end() ||
           findAxisAlignedDistanceConstraint(id) != m_axisAlignedDistanceConstraints.end() ||
           findAngleConstraint(id) != m_angleConstraints.end() ||
           findEqualDistanceConstraint(id) != m_equalDistanceConstraints.end() ||
           findPointOnLineConstraint(id) != m_pointOnLineConstraints.end() ||
           findSketchPlaneConstraint(id) != m_sketchPlaneConstraints.end();
}

size_t ConstraintGraph::totalConstraintCount() const noexcept
{
    return m_distanceConstraints.size() +
           m_coincidentConstraints.size() +
           m_axisAlignedDistanceConstraints.size() +
           m_angleConstraints.size() +
           m_equalDistanceConstraints.size() +
           m_pointOnLineConstraints.size() +
           m_sketchPlaneConstraints.size();
}

DegreesOfFreedom ConstraintGraph::analyzeDegreesOfFreedom() const noexcept
{
    DegreesOfFreedom dof{};
    dof.freeVariables = static_cast<int32_t>(m_entities.size()) * 3;
    dof.effectiveConstraints = 0;

    dof.effectiveConstraints += static_cast<int32_t>(m_distanceConstraints.size()) * 1;
    dof.effectiveConstraints += static_cast<int32_t>(m_coincidentConstraints.size()) * 3;
    dof.effectiveConstraints += static_cast<int32_t>(m_axisAlignedDistanceConstraints.size()) * 1;
    dof.effectiveConstraints += static_cast<int32_t>(m_angleConstraints.size()) * 1;
    dof.effectiveConstraints += static_cast<int32_t>(m_equalDistanceConstraints.size()) * 1;
    dof.effectiveConstraints += static_cast<int32_t>(m_pointOnLineConstraints.size()) * 2;
    dof.effectiveConstraints += static_cast<int32_t>(m_sketchPlaneConstraints.size()) * 1;

    dof.estimatedRemainingDOF = dof.freeVariables - dof.effectiveConstraints;
    dof.likelyOverconstrained = dof.estimatedRemainingDOF < 0;
    return dof;
}

// ── find implementations ────────────────────────────────────────────────────

std::vector<ParametricEntity>::iterator ConstraintGraph::findEntity(ParametricEntityId id) noexcept
{
    return std::find_if(m_entities.begin(),
                        m_entities.end(),
                        [id](const ParametricEntity& e) { return e.id == id; });
}

std::vector<ParametricEntity>::const_iterator ConstraintGraph::findEntity(ParametricEntityId id) const noexcept
{
    return std::find_if(m_entities.begin(),
                        m_entities.end(),
                        [id](const ParametricEntity& e) { return e.id == id; });
}

std::vector<DistanceConstraint>::iterator ConstraintGraph::findConstraint(ParametricConstraintId id) noexcept
{
    return std::find_if(m_distanceConstraints.begin(),
                        m_distanceConstraints.end(),
                        [id](const DistanceConstraint& c) { return c.id == id; });
}

std::vector<DistanceConstraint>::const_iterator ConstraintGraph::findConstraint(ParametricConstraintId id) const noexcept
{
    return std::find_if(m_distanceConstraints.begin(),
                        m_distanceConstraints.end(),
                        [id](const DistanceConstraint& c) { return c.id == id; });
}

std::vector<CoincidentConstraint>::iterator ConstraintGraph::findCoincidentConstraint(ParametricConstraintId id) noexcept
{
    return std::find_if(m_coincidentConstraints.begin(),
                        m_coincidentConstraints.end(),
                        [id](const CoincidentConstraint& c) { return c.id == id; });
}

std::vector<CoincidentConstraint>::const_iterator ConstraintGraph::findCoincidentConstraint(ParametricConstraintId id) const noexcept
{
    return std::find_if(m_coincidentConstraints.begin(),
                        m_coincidentConstraints.end(),
                        [id](const CoincidentConstraint& c) { return c.id == id; });
}

std::vector<AxisAlignedDistanceConstraint>::iterator ConstraintGraph::findAxisAlignedDistanceConstraint(ParametricConstraintId id) noexcept
{
    return std::find_if(m_axisAlignedDistanceConstraints.begin(),
                        m_axisAlignedDistanceConstraints.end(),
                        [id](const AxisAlignedDistanceConstraint& c) { return c.id == id; });
}

std::vector<AxisAlignedDistanceConstraint>::const_iterator ConstraintGraph::findAxisAlignedDistanceConstraint(ParametricConstraintId id) const noexcept
{
    return std::find_if(m_axisAlignedDistanceConstraints.begin(),
                        m_axisAlignedDistanceConstraints.end(),
                        [id](const AxisAlignedDistanceConstraint& c) { return c.id == id; });
}

std::vector<AngleConstraint>::iterator ConstraintGraph::findAngleConstraint(ParametricConstraintId id) noexcept
{
    return std::find_if(m_angleConstraints.begin(),
                        m_angleConstraints.end(),
                        [id](const AngleConstraint& c) { return c.id == id; });
}

std::vector<AngleConstraint>::const_iterator ConstraintGraph::findAngleConstraint(ParametricConstraintId id) const noexcept
{
    return std::find_if(m_angleConstraints.begin(),
                        m_angleConstraints.end(),
                        [id](const AngleConstraint& c) { return c.id == id; });
}

std::vector<EqualDistanceConstraint>::iterator ConstraintGraph::findEqualDistanceConstraint(ParametricConstraintId id) noexcept
{
    return std::find_if(m_equalDistanceConstraints.begin(),
                        m_equalDistanceConstraints.end(),
                        [id](const EqualDistanceConstraint& c) { return c.id == id; });
}

std::vector<EqualDistanceConstraint>::const_iterator ConstraintGraph::findEqualDistanceConstraint(ParametricConstraintId id) const noexcept
{
    return std::find_if(m_equalDistanceConstraints.begin(),
                        m_equalDistanceConstraints.end(),
                        [id](const EqualDistanceConstraint& c) { return c.id == id; });
}

std::vector<PointOnLineConstraint>::iterator ConstraintGraph::findPointOnLineConstraint(ParametricConstraintId id) noexcept
{
    return std::find_if(m_pointOnLineConstraints.begin(),
                        m_pointOnLineConstraints.end(),
                        [id](const PointOnLineConstraint& c) { return c.id == id; });
}

std::vector<PointOnLineConstraint>::const_iterator ConstraintGraph::findPointOnLineConstraint(ParametricConstraintId id) const noexcept
{
    return std::find_if(m_pointOnLineConstraints.begin(),
                        m_pointOnLineConstraints.end(),
                        [id](const PointOnLineConstraint& c) { return c.id == id; });
}

std::vector<SketchPlaneConstraint>::iterator ConstraintGraph::findSketchPlaneConstraint(ParametricConstraintId id) noexcept
{
    return std::find_if(m_sketchPlaneConstraints.begin(),
                        m_sketchPlaneConstraints.end(),
                        [id](const SketchPlaneConstraint& c) { return c.id == id; });
}

std::vector<SketchPlaneConstraint>::const_iterator ConstraintGraph::findSketchPlaneConstraint(ParametricConstraintId id) const noexcept
{
    return std::find_if(m_sketchPlaneConstraints.begin(),
                        m_sketchPlaneConstraints.end(),
                        [id](const SketchPlaneConstraint& c) { return c.id == id; });
}

// ── Constraint combinators ───────────────────────────────────────

ParametricConstraintId ConstraintGraph::addParallelConstraint(ParametricEntityId a0,ParametricEntityId a1,ParametricEntityId b0,ParametricEntityId b1) noexcept
{ (void)b1; return addAngleConstraint(a0,a1,b0,0.0); }

ParametricConstraintId ConstraintGraph::addPerpendicularConstraint(ParametricEntityId a0,ParametricEntityId a1,ParametricEntityId b0,ParametricEntityId b1) noexcept
{ (void)b1; return addAngleConstraint(a0,a1,b0,90.0); }

ParametricConstraintId ConstraintGraph::addCollinearConstraint(ParametricEntityId a0,ParametricEntityId a1,ParametricEntityId b0,ParametricEntityId b1) noexcept
{ (void)addPointOnLineConstraint(b0,a0,a1); (void)addPointOnLineConstraint(b1,a0,a1); return addAngleConstraint(a0,a1,b0,0.0); }

ParametricConstraintId ConstraintGraph::addHorizontalConstraint(ParametricEntityId a,ParametricEntityId b) noexcept
{ return addAxisAlignedDistanceConstraint(a,b,Axis::Y,0.0); }

ParametricConstraintId ConstraintGraph::addVerticalConstraint(ParametricEntityId a,ParametricEntityId b) noexcept
{ return addAxisAlignedDistanceConstraint(a,b,Axis::X,0.0); }

ParametricConstraintId ConstraintGraph::addConcentricConstraint(ParametricEntityId ca,ParametricEntityId cb) noexcept
{ return addCoincidentConstraint(ca,cb); }

ParametricConstraintId ConstraintGraph::addMidpointConstraint(ParametricEntityId p,ParametricEntityId a,ParametricEntityId b) noexcept
{
    ParametricPoint3 pa{},pb{};
    const auto* ppa=point(a); const auto* ppb=point(b);
    if(ppa){pa=*ppa;} if(ppb){pb=*ppb;}
    double dx=pb.x-pa.x,dy=pb.y-pa.y,dz=pb.z-pa.z;
    double hl=std::sqrt(dx*dx+dy*dy+dz*dz)*0.5;
    ParametricEntityId mid=addPoint({(pa.x+pb.x)*0.5,(pa.y+pb.y)*0.5,(pa.z+pb.z)*0.5});
    if(mid==kInvalidEntityId)return kInvalidConstraintId;
    (void)addCoincidentConstraint(p,mid);(void)addDistanceConstraint(a,mid,hl);
    (void)addDistanceConstraint(b,mid,hl);return addPointOnLineConstraint(mid,a,b);
}

ParametricConstraintId ConstraintGraph::addSymmetricConstraint(ParametricEntityId a,ParametricEntityId b,ParametricEntityId l0,ParametricEntityId l1) noexcept
{
    ParametricPoint3 pa{},pb{};
    const auto* ppa=point(a);const auto* ppb=point(b);
    if(ppa){pa=*ppa;}if(ppb){pb=*ppb;}
    ParametricEntityId mid=addPoint({(pa.x+pb.x)*0.5,(pa.y+pb.y)*0.5,(pa.z+pb.z)*0.5});
    if(mid==kInvalidEntityId)return kInvalidConstraintId;
    (void)addPointOnLineConstraint(mid,l0,l1);
    return addPerpendicularConstraint(a,b,l0,l1);
}

ParametricConstraintId ConstraintGraph::addMateConstraint(ParametricEntityId fa0,ParametricEntityId fa1,ParametricEntityId fa2,ParametricEntityId fb0,ParametricEntityId fb1,ParametricEntityId fb2) noexcept
{ (void)addCoincidentConstraint(fa0,fb0);(void)addCoincidentConstraint(fa1,fb1);return addCoincidentConstraint(fa2,fb2); }

ParametricConstraintId ConstraintGraph::addAlignConstraint(ParametricEntityId axA0,ParametricEntityId axA1,ParametricEntityId axB0,ParametricEntityId axB1) noexcept
{ (void)addPointOnLineConstraint(axB0,axA0,axA1);return addPointOnLineConstraint(axB1,axA0,axA1); }

ParametricConstraintId ConstraintGraph::addGearConstraint(ParametricEntityId ca,ParametricEntityId cb,double rA,double rB) noexcept
{ return addDistanceConstraint(ca,cb,rA+rB); }

} // namespace nexus::parametric
