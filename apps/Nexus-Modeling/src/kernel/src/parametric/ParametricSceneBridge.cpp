#include <nexus/parametric/ParametricSceneBridge.h>

namespace nexus::parametric {

void applySolutionToNodePositions(const ConstraintGraph& graph,
                                  nexus::render::SceneGraph& scene) noexcept
{
    scene.traverse([&](nexus::render::Node& node, const nexus::render::Mat4&) {
        if (node.parametricBindingId == 0) return;

        const ParametricPoint3* pt = graph.point(node.parametricBindingId);
        if (!pt) return;

        nexus::render::Transform& tf = node.localTransform();
        tf.translation.x = static_cast<float>(pt->x);
        tf.translation.y = static_cast<float>(pt->y);
        tf.translation.z = static_cast<float>(pt->z);
        node.markDirty();
    });
}

} // namespace nexus::parametric
