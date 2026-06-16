#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Viewer Module
//
//  Manages the 3D viewport: builds SceneGraph from CAD features, handles
//  camera orbit/pan/zoom, and renders mode-specific overlays.
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadSelection.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/render/Camera.h>

#include <string>
#include <vector>

namespace nexus::cad {

using Vec3 = nexus::render::Vec3;

// ── Viewport Camera ──────────────────────────────────────────────────────

class ViewportCamera {
public:
    void orbit(float deltaX, float deltaY);
    void pan(float deltaX, float deltaY);
    void zoom(float delta);
    void reset();

    void lookAt(const Vec3& target, float distance = 20.f);

    [[nodiscard]] Vec3 position() const noexcept { return m_position; }
    [[nodiscard]] Vec3 target()   const noexcept { return m_target; }
    [[nodiscard]] Vec3 up()       const noexcept { return m_up; }
    [[nodiscard]] float distance() const noexcept { return m_distance; }

    // Standard views.
    void viewFront();
    void viewTop();
    void viewRight();
    void viewIsometric();

private:
    Vec3 m_position{0, 10, 20};
    Vec3 m_target{0, 0, 0};
    Vec3 m_up{0, 1, 0};
    float m_distance = 22.36f;
    float m_yaw = 0.f;
    float m_pitch = 0.46f;
};

// ── Viewer ────────────────────────────────────────────────────────────────

class CadViewer {
public:
    explicit CadViewer(CadDocument& doc, CadSelection& sel);

    // Update the scene graph from the document.
    void refreshScene();

    // Apply the current camera to the scene.
    void updateCamera();

    // Highlight the current selection.
    void highlightSelection();

    // Access.
    [[nodiscard]] ViewportCamera& camera() noexcept { return m_camera; }
    [[nodiscard]] nexus::render::SceneGraph* scene() noexcept { return &m_scene; }
    [[nodiscard]] const nexus::render::SceneGraph* scene() const noexcept { return &m_scene; }
    [[nodiscard]] bool hasContent() const noexcept { return m_hasContent; }

private:
    CadDocument& m_doc;
    CadSelection& m_selection;
    nexus::render::SceneGraph m_scene;
    ViewportCamera m_camera;
    bool m_hasContent = false;
};

} // namespace nexus::cad
