#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Render Bridge
//
//  Connects CadDocument features to the rendering SceneGraph so that
//  CAD geometry can be visualized in real-time.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/render/SceneGraph.h>

namespace nexus::cad {

class CadRenderBridge {
public:
    // Populate a scene graph from a CAD document.
    // Each feature with a valid mesh gets a scene node.
    [[nodiscard]] static uint32_t populateScene(
        const CadDocument&     doc,
        nexus::render::SceneGraph& scene) noexcept;

    // Update scene nodes from the document (after rebuild).
    static void updateScene(
        const CadDocument&     doc,
        nexus::render::SceneGraph& scene) noexcept;

    // Highlight selected features in the scene.
    static void applySelection(
        const CadSelection&    selection,
        nexus::render::SceneGraph& scene) noexcept;
};

} // namespace nexus::cad
