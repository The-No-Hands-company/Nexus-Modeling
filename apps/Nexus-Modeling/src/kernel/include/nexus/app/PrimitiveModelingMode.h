#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Primitive Modeling Mode
//
//  Interactive placement and manipulation of geometric primitives (box,
//  sphere, cylinder, cone, torus, plane).  Uses the existing geometry
//  kernel primitives and the scene graph for visualization.
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/app/AppMode.h>
#include <nexus/geometry/Mesh.h>

#include <string>
#include <vector>

namespace nexus::app {

class PrimitiveModelingMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override { return "modeling"; }
    [[nodiscard]] std::string displayName() const override { return "Modeling"; }

    void onActivate(AppContext& ctx) override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    void renderToolVisuals(AppContext& ctx) override;

    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
    bool executeAction(const std::string& actionId, AppContext& ctx) override;

private:
    enum class PrimitiveType { Box, Sphere, Cylinder, Cone, Torus, Plane };
    PrimitiveType m_activePrimitive = PrimitiveType::Box;
    bool m_placing = false;

    geometry::Mesh createPrimitive(PrimitiveType type) const noexcept;
    std::string primitiveName(PrimitiveType type) const noexcept;
};

} // namespace nexus::app
