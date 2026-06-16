#include <nexus/cad/CadViewer.h>
#include <cmath>
#include <numbers>

namespace nexus::cad {

// ── ViewportCamera ──────────────────────────────────────────────────────

void ViewportCamera::orbit(float dx, float dy) {
    m_yaw   += dx * 0.01f;
    m_pitch += dy * 0.01f;
    m_pitch = std::clamp(m_pitch, -1.5f, 1.5f);

    m_position.x = m_target.x + m_distance * std::cos(m_pitch) * std::sin(m_yaw);
    m_position.y = m_target.y + m_distance * std::sin(m_pitch);
    m_position.z = m_target.z + m_distance * std::cos(m_pitch) * std::cos(m_yaw);
}

void ViewportCamera::pan(float dx, float dy) {
    Vec3 forward = (m_target - m_position);
    float fLen = forward.length();
    if(fLen < 1e-10f) return;
    forward = forward * (1.f / fLen);
    Vec3 right = forward.cross(m_up).normalize();
    Vec3 up = right.cross(forward).normalize();

    float panSpeed = m_distance * 0.001f;
    Vec3 offset = right * (-dx * panSpeed) + up * (dy * panSpeed);
    m_target = m_target + offset;
    m_position = m_position + offset;
}

void ViewportCamera::zoom(float delta) {
    m_distance = std::clamp(m_distance * (1.f - delta * 0.001f), 0.1f, 1000.f);
    m_position.x = m_target.x + m_distance * std::cos(m_pitch) * std::sin(m_yaw);
    m_position.y = m_target.y + m_distance * std::sin(m_pitch);
    m_position.z = m_target.z + m_distance * std::cos(m_pitch) * std::cos(m_yaw);
}

void ViewportCamera::reset() { lookAt({0,0,0}, 20.f); }
void ViewportCamera::lookAt(const Vec3& t, float d) { m_target=t; m_distance=d; m_position={t.x,t.y+d*0.5f,t.z+d}; m_yaw=0; m_pitch=0.46f; }

void ViewportCamera::viewFront()  { m_position={m_target.x,m_target.y,m_target.z+m_distance}; m_up={0,1,0}; }
void ViewportCamera::viewTop()    { m_position={m_target.x,m_target.y+m_distance,m_target.z}; m_up={0,0,-1}; m_pitch=1.5f; }
void ViewportCamera::viewRight()  { m_position={m_target.x+m_distance,m_target.y,m_target.z}; m_up={0,1,0}; m_yaw=1.57f; }
void ViewportCamera::viewIsometric() { reset(); m_position={m_distance*0.7f,m_distance*0.5f,m_distance*0.7f}; }

// ── CadViewer ──────────────────────────────────────────────────────────

CadViewer::CadViewer(CadDocument& doc, CadSelection& sel) : m_doc(doc), m_selection(sel) {}

void CadViewer::refreshScene() {
    m_hasContent = false;

    // SceneGraph is non-copyable. Nodes persist; we track if any exist.
    for(size_t i=1; i<=m_doc.history().featureCount(); ++i) {
        auto* node = m_doc.history().node(static_cast<parametric::FeatureId>(i));
        if(node && node->mesh) m_hasContent = true;
    }
}

void CadViewer::updateCamera() {
    // Camera is passed to the renderer at draw time, not stored in SceneGraph.
    (void)this; // handled by renderer externally
}

void CadViewer::highlightSelection() {
    for(const auto& item : m_selection.items()) {
        auto* sceneNode = m_scene.findNode("Feature_"+std::to_string(item.index));
        if(sceneNode) sceneNode->markDirty();
    }
}

} // namespace nexus::cad
