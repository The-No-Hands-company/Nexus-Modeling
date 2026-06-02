#include <nexus/parametric/ConstraintGraph.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <map>
#include <queue>

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

    // Keep graph validity explicit by removing constraints that referenced the erased entity.
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

bool ConstraintGraph::removeConstraint(ParametricConstraintId id) noexcept
{
    const auto it = findConstraint(id);
    if (it != m_distanceConstraints.end()) {
        m_distanceConstraints.erase(it);
        return true;
    }

    const auto coincidentIt = findCoincidentConstraint(id);
    if (coincidentIt != m_coincidentConstraints.end()) {
        m_coincidentConstraints.erase(coincidentIt);
        return true;
    }

    const auto axisIt = findAxisAlignedDistanceConstraint(id);
    if (axisIt != m_axisAlignedDistanceConstraints.end()) {
        m_axisAlignedDistanceConstraints.erase(axisIt);
        return true;
    }

    return false;
}

bool ConstraintGraph::hasConstraint(ParametricConstraintId id) const noexcept
{
    return findConstraint(id) != m_distanceConstraints.end() ||
           findCoincidentConstraint(id) != m_coincidentConstraints.end() ||
           findAxisAlignedDistanceConstraint(id) != m_axisAlignedDistanceConstraints.end();
}

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

std::vector<ParametricConstraintId> ConstraintGraph::buildDependencyOrder() const noexcept
{
    // Build a directed constraint dependency graph: each constraint (A→B) adds
    // an edge from A's last-modifying constraint to this constraint.
    // We model it simply: for each entity, collect the constraint IDs that
    // produce it (write to entityB), then sort by in-degree (Kahn's algorithm).

    // Flatten all constraints into a list of (id, entityA, entityB) triples.
    struct CEdge {
        ParametricConstraintId id;
        ParametricEntityId from; // entityA
        ParametricEntityId to;   // entityB
    };
    std::vector<CEdge> edges;
    for (const auto& c : m_distanceConstraints) {
        edges.push_back({c.id, c.entityA, c.entityB});
    }
    for (const auto& c : m_coincidentConstraints) {
        edges.push_back({c.id, c.entityA, c.entityB});
    }
    for (const auto& c : m_axisAlignedDistanceConstraints) {
        edges.push_back({c.id, c.entityA, c.entityB});
    }

    if (edges.empty()) {
        return {};
    }

    // Build entity → list of constraint IDs that have this entity as entityB.
    // In-degree of each constraint = number of constraints writing to its entityA.
    std::map<ParametricEntityId, std::vector<size_t>> entityToConsumers; // entity → edge indices
    std::map<ParametricEntityId, std::vector<size_t>> entityToProducers; // entity → edge indices that write to entity

    for (size_t i = 0; i < edges.size(); ++i) {
        entityToConsumers[edges[i].from].push_back(i);
        entityToProducers[edges[i].to].push_back(i);
    }

    // In-degree of constraint i = how many constraints produce edges[i].from
    std::vector<int> inDegree(edges.size(), 0);
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto it = entityToProducers.find(edges[i].from);
        if (it != entityToProducers.end()) {
            inDegree[i] = static_cast<int>(it->second.size());
        }
    }

    // Kahn's algorithm: start with constraints whose entityA has no producer.
    std::queue<size_t> ready;
    for (size_t i = 0; i < edges.size(); ++i) {
        if (inDegree[i] == 0) {
            ready.push(i);
        }
    }

    std::vector<ParametricConstraintId> order;
    order.reserve(edges.size());

    while (!ready.empty()) {
        const size_t cur = ready.front();
        ready.pop();
        order.push_back(edges[cur].id);

        // All constraints whose entityA is edges[cur].to now have one fewer dependency.
        const auto it = entityToConsumers.find(edges[cur].to);
        if (it != entityToConsumers.end()) {
            for (size_t j : it->second) {
                if (--inDegree[j] == 0) {
                    ready.push(j);
                }
            }
        }
    }

    return order;
}

std::vector<ParametricConstraintId> ConstraintGraph::cycleMembers() const noexcept
{
    const auto order = buildDependencyOrder();

    // Any constraint not in the topological order is part of a cycle.
    struct CId { ParametricConstraintId id; };
    std::vector<ParametricConstraintId> all;
    for (const auto& c : m_distanceConstraints)           { all.push_back(c.id); }
    for (const auto& c : m_coincidentConstraints)         { all.push_back(c.id); }
    for (const auto& c : m_axisAlignedDistanceConstraints){ all.push_back(c.id); }

    std::vector<ParametricConstraintId> cycles;
    for (auto id : all) {
        if (std::find(order.begin(), order.end(), id) == order.end()) {
            cycles.push_back(id);
        }
    }
    return cycles;
}

DOFAnalysis ConstraintGraph::analyseDOF() const noexcept
{
    DOFAnalysis analysis;

    const int n = static_cast<int>(m_entities.size());
    analysis.totalDOF = 3 * n;

    // DOF cost per constraint type (see v0.26-planning.md DOF Cost Model).
    const int coincidentCost        = 3;
    const int distanceCost          = 1;
    const int axisAlignedCost       = 1;

    analysis.consumedDOF =
        static_cast<int>(m_coincidentConstraints.size()) * coincidentCost +
        static_cast<int>(m_distanceConstraints.size())   * distanceCost   +
        static_cast<int>(m_axisAlignedDistanceConstraints.size()) * axisAlignedCost;

    analysis.remainingDOF = analysis.totalDOF - analysis.consumedDOF;

    // Redundancy scan: detect duplicate constraint pairs.
    uint32_t redundant = 0;

    // Distance: same (min,max) entity pair and same targetDistance.
    for (size_t i = 0; i < m_distanceConstraints.size(); ++i) {
        for (size_t j = i + 1; j < m_distanceConstraints.size(); ++j) {
            const auto& a = m_distanceConstraints[i];
            const auto& b = m_distanceConstraints[j];
            const bool sameEntities =
                (a.entityA == b.entityA && a.entityB == b.entityB) ||
                (a.entityA == b.entityB && a.entityB == b.entityA);
            if (sameEntities && a.targetDistance == b.targetDistance) {
                ++redundant;
            }
        }
    }

    // Coincident: same entity pair.
    for (size_t i = 0; i < m_coincidentConstraints.size(); ++i) {
        for (size_t j = i + 1; j < m_coincidentConstraints.size(); ++j) {
            const auto& a = m_coincidentConstraints[i];
            const auto& b = m_coincidentConstraints[j];
            const bool sameEntities =
                (a.entityA == b.entityA && a.entityB == b.entityB) ||
                (a.entityA == b.entityB && a.entityB == b.entityA);
            if (sameEntities) {
                ++redundant;
            }
        }
    }

    // AxisAligned: same entity pair, same axis, same targetDistance.
    for (size_t i = 0; i < m_axisAlignedDistanceConstraints.size(); ++i) {
        for (size_t j = i + 1; j < m_axisAlignedDistanceConstraints.size(); ++j) {
            const auto& a = m_axisAlignedDistanceConstraints[i];
            const auto& b = m_axisAlignedDistanceConstraints[j];
            const bool sameEntities =
                (a.entityA == b.entityA && a.entityB == b.entityB) ||
                (a.entityA == b.entityB && a.entityB == b.entityA);
            if (sameEntities && a.axis == b.axis && a.targetDistance == b.targetDistance) {
                ++redundant;
            }
        }
    }

    analysis.redundantConstraintCount = redundant;

    // Classify status.
    if (n == 0) {
        analysis.status = ConstraintStatus::Unconstrained;
    } else if (analysis.remainingDOF < 0 || redundant > 0) {
        analysis.status = ConstraintStatus::OverConstrained;
    } else if (analysis.remainingDOF == 0) {
        analysis.status = ConstraintStatus::WellConstrained;
    } else {
        analysis.status = ConstraintStatus::UnderConstrained;
    }

    return analysis;
}

} // namespace nexus::parametric
