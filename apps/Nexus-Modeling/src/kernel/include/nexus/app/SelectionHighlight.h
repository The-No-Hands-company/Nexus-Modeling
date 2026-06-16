#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Selection Highlight
//
//  Renders a highlight overlay around selected objects.
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/render/Camera.h>

namespace nexus::app {

class SelectionHighlight {
public:
    // Draw a highlight wireframe around a feature's mesh.
    void render(const nexus::cad::CadDocument& doc,
                nexus::parametric::FeatureId selectedId) const noexcept;
};

} // namespace nexus::app
