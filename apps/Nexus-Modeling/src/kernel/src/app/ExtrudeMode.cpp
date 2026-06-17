#include <nexus/app/ExtrudeMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/CurveExtrude.h>
#include <cstdio>

namespace nexus::app {

[[nodiscard]] std::string ExtrudeMode::modeId() const { return "extrude"; }
[[nodiscard]] std::string ExtrudeMode::displayName() const { return "Extrude"; }
[[nodiscard]] std::vector<ActionDescriptor> ExtrudeMode::actions() const {
    return {{"extrude.create","Extrude","E","modify","Extrude"}};
}
EventResult ExtrudeMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(!ctx.document) return EventResult::Unconsumed;
    auto& hist = ctx.document->history();
    size_t fc = hist.featureCount();
    if(fc==0 || m_sketchId == parametric::kInvalidFeatureId) {
        for(parametric::FeatureId i=static_cast<parametric::FeatureId>(fc); i>=1; --i) {
            auto* n = hist.node(i);
            if(n && n->kind==parametric::FeatureKind::Sketch) { m_sketchId=i; break; }
        }
        if(m_sketchId==parametric::kInvalidFeatureId) return EventResult::Unconsumed;
        printf("Extrude: sketch feature %u ready (drag to set height)\n", m_sketchId);
    }
    if(event.type==InputEventType::MouseDown && event.button==0) {
        m_dragging=true; m_startY=event.position.y; return EventResult::Consumed;
    }
    if(event.type==InputEventType::MouseDrag && m_dragging) {
        float dy = event.position.y - m_startY;
        float height = std::max(0.1f, -dy * 0.02f);
        printf("Extrude height: %.2f\r", height); fflush(stdout);
        return EventResult::Consumed;
    }
    if(event.type==InputEventType::MouseUp && m_dragging) {
        m_dragging=false;
        ctx.previewMesh.reset();
        float dy = event.position.y - m_startY;
        float h = std::max(0.1f, -dy * 0.02f);
        geometry::CurveExtrudeDesc desc;
        desc.direction={0,0,1}; desc.height=h; desc.capEnds=true;
        auto fid = ctx.document->addExtrude(m_sketchId, desc);
        if(fid!=parametric::kInvalidFeatureId) { hist.rebuild(); printf("\nExtrude: feature %u (h=%.2f)\n",fid,h); }
        m_sketchId=parametric::kInvalidFeatureId;
        return EventResult::Consumed;
    }
    return EventResult::Unconsumed;
}

bool ExtrudeMode::onDeactivate(AppContext&) {
    m_dragging = false;
    m_sketchId = parametric::kInvalidFeatureId;
    return true;
}

} // namespace nexus::app
