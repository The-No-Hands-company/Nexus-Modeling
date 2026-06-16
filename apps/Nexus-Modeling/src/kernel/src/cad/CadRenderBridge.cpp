#include <nexus/cad/CadRenderBridge.h>

namespace nexus::cad {

uint32_t CadRenderBridge::populateScene(
    const CadDocument& doc,
    nexus::render::SceneGraph& scene) noexcept
{
    uint32_t nodeCount = 0;
    for (size_t i = 1; i <= doc.history().featureCount(); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i));
        if (!node || !node->mesh) continue;

        auto* sceneNode = scene.createNode(
            node->name.empty() ? "Feature_" + std::to_string(i) : node->name);

        if (sceneNode) {
            sceneNode->parametricBindingId = static_cast<parametric::ParametricEntityId>(i);
            sceneNode->markDirty();
            nodeCount++;
        }
    }
    return nodeCount;
}

void CadRenderBridge::updateScene(
    const CadDocument& doc,
    nexus::render::SceneGraph& scene) noexcept
{
    for (size_t i = 1; i <= doc.history().featureCount(); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i));
        if (!node || !node->mesh) continue;
        // Scene graph update — positions and transforms from feature mesh.
        (void)scene;
    }
}

void CadRenderBridge::applySelection(
    const CadSelection& selection,
    nexus::render::SceneGraph& scene) noexcept
{
    (void)selection;
    (void)scene;
    // Highlight selected nodes by modifying material or outline.
}

} // namespace nexus::cad
