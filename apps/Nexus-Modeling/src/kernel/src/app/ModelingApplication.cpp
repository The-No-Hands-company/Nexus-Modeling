#include <nexus/app/ModelingApplication.h>
#include <iostream>

namespace nexus::app {

ModelingApplication::ModelingApplication() : m_orchestrator(m_context) {
    m_context.document = &m_document;
    m_context.selection = &m_selection;
    m_context.scene = &m_scene;
    m_context.orchestrator = &m_orchestrator;
}
ModelingApplication::~ModelingApplication() { shutdown(); }

bool ModelingApplication::initialize() {
    m_orchestrator.registerBuiltinModes();
    m_orchestrator.switchTo("select");
    m_initialized = true;
    m_running = true;
    std::cout << "Nexus Modeling — initialized. Mode: " 
              << m_orchestrator.activeModeId() << std::endl;
    return true;
}

void ModelingApplication::run() { if(m_initialized) render(); }
void ModelingApplication::shutdown() { m_running = false; }

void ModelingApplication::onMouseDown(int b, float x, float y) {
    InputEvent ev; ev.type=InputEventType::MouseDown; ev.button=b; ev.position={x,y,0};
    m_orchestrator.routeInput(ev);
}
void ModelingApplication::onMouseUp(int b, float x, float y) {
    InputEvent ev; ev.type=InputEventType::MouseUp; ev.button=b; ev.position={x,y,0};
    m_orchestrator.routeInput(ev);
}
void ModelingApplication::onMouseMove(float x, float y) {
    InputEvent ev; ev.type=InputEventType::MouseMove; ev.position={x,y,0};
    m_orchestrator.routeInput(ev);
}
void ModelingApplication::onKeyDown(int k) {
    InputEvent ev; ev.type=InputEventType::KeyDown; ev.key=k;
    m_orchestrator.routeInput(ev);
}
void ModelingApplication::render() {
    m_orchestrator.renderOverlays();
    m_orchestrator.renderToolVisuals();
}

} // namespace nexus::app
