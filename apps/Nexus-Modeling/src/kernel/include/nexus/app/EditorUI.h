#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Editor UI (Menus, Toolbar, Context Menu)
//
//  ImGui-based menu bar, toolbar, and right-click context menu.
//  Keyboard shortcuts remain functional alongside menu items.
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/app/AppMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadSelection.h>
#include <nexus/app/ViewportGrid.h>
#include <nexus/app/TransformGizmo.h>

struct GLFWwindow;

namespace nexus::app {

using FeatureId = nexus::parametric::FeatureId;

class EditorUI {
public:
    // Initialize ImGui with the GLFW window.
    static void initialize(GLFWwindow* window);

    // Shutdown ImGui.
    static void shutdown();

    // Begin a new UI frame.
    static void beginFrame();

    // End the UI frame and render.
    static void endFrame();

    // ── Menu bar ──────────────────────────────────────────────────
    // Returns true if the user triggered an action.
    static bool renderMenuBar(AppContext& ctx, TransformGizmo& gizmo,
                               ViewportController& viewport);

    // ── Toolbar ──────────────────────────────────────────────────
    static void renderToolbar(AppContext& ctx, TransformGizmo& gizmo);

    // ── Right-click context menu ─────────────────────────────────
    static void renderContextMenu(AppContext& ctx, TransformGizmo& gizmo,
                                   FeatureId selectedId);

    // ── Status bar ────────────────────────────────────────────────
    static void renderStatusBar(const AppContext& ctx, bool snapToGrid,
                                 FeatureId selectedId);

    // ── Outliner (object list) ─────────────────────────────────────
    static void renderOutliner(AppContext& ctx, FeatureId& selectedId);

    // ── Properties panel ───────────────────────────────────────────
    static void renderProperties(AppContext& ctx, FeatureId selectedId);

    // ── Animation timeline ──────────────────────────────────────────
    static void renderTimeline(FeatureId selectedId, bool& playing,
                                int& frame, int maxFrame);
};

} // namespace nexus::app
