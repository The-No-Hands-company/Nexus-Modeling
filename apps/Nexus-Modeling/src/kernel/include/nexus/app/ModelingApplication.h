#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Application
//
//  Top-level application for Nexus Modeling DCC.  Owns the mode orchestrator,
//  document, selection, and scene graph.  CAD is one layer among many:
//  geometry → CAD → animation → simulation → sculpting → rendering.
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/app/AppMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadSelection.h>
#include <nexus/render/SceneGraph.h>

namespace nexus::app {

class ModelingApplication {
public:
    ModelingApplication();
    ~ModelingApplication();

    bool initialize();
    void run();
    void shutdown();

    [[nodiscard]] cad::CadDocument&   document()    noexcept { return m_document; }
    [[nodiscard]] cad::CadSelection&  selection()   noexcept { return m_selection; }
    [[nodiscard]] ModeOrchestrator&   orchestrator() noexcept { return m_orchestrator; }
    [[nodiscard]] AppContext&         context()     noexcept { return m_context; }

    void onMouseDown(int button, float x, float y);
    void onMouseUp(int button, float x, float y);
    void onMouseMove(float x, float y);
    void onKeyDown(int key);
    void render();

private:
    cad::CadDocument           m_document;
    cad::CadSelection          m_selection;
    nexus::render::SceneGraph  m_scene;
    AppContext                 m_context;
    ModeOrchestrator           m_orchestrator;
    bool m_running = false;
    bool m_initialized = false;
};

} // namespace nexus::app
