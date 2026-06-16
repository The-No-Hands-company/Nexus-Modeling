#pragma once

#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/render/SceneGraph.h>

namespace nexus::parametric {

// Apply a solved constraint graph to scene node positions.
// For each scene node with a non-zero parametricBindingId, if that ID matches
// a ParametricEntity in the graph, the node's translation is set to the
// parametric point position.
void applySolutionToNodePositions(const ConstraintGraph& graph,
                                  nexus::render::SceneGraph& scene) noexcept;

} // namespace nexus::parametric
