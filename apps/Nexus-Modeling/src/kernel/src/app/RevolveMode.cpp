#include <nexus/app/RevolveMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <cmath>
#include <cstdio>

namespace nexus::app {

[[nodiscard]] std::string RevolveMode::modeId() const { return "revolve"; }
[[nodiscard]] std::string RevolveMode::displayName() const { return "Revolve"; }
[[nodiscard]] std::vector<ActionDescriptor> RevolveMode::actions() const {
    return {{"revolve.create","Revolve","R","modify","Revolve"}};
}
EventResult RevolveMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(!ctx.document) return EventResult::Unconsumed;
    auto& hist = ctx.document->history();
    size_t fc = hist.featureCount();
    if(fc==0 || m_sketchId==parametric::kInvalidFeatureId) {
        for(parametric::FeatureId i=static_cast<parametric::FeatureId>(fc); i>=1; --i) {
            auto* n=hist.node(i); if(n&&n->kind==parametric::FeatureKind::Sketch){m_sketchId=i;break;}
        }
        if(m_sketchId==parametric::kInvalidFeatureId) return EventResult::Unconsumed;
    }
    if(event.type==InputEventType::MouseDown && event.button==0) {
        m_dragging=true; m_startY=event.position.y; return EventResult::Consumed;
    }
    if(event.type==InputEventType::MouseDrag && m_dragging) {
        float angle = (event.position.y - m_startY) * 2.f;
        printf("Revolve angle: %.0f deg\r", angle); fflush(stdout);
        return EventResult::Consumed;
    }
    if(event.type==InputEventType::MouseUp && m_dragging) {
        m_dragging=false;
        float angle = (event.position.y - m_startY) * 2.f;
        if(std::fabs(angle)<1.f) angle=360.f;
        geometry::RevolveDesc desc;
        desc.axisDirection={0,1,0}; desc.endAngleDeg=angle; desc.capEnds=true;
        auto fid = ctx.document->addRevolve(m_sketchId, desc);
        if(fid!=parametric::kInvalidFeatureId){hist.rebuild();printf("\nRevolve: feature %u (%.1f deg)\n",fid,angle);}
        m_sketchId=parametric::kInvalidFeatureId;
        return EventResult::Consumed;
    }
    return EventResult::Unconsumed;
}

bool RevolveMode::onDeactivate(AppContext&) {
    m_dragging = false;
    m_sketchId = parametric::kInvalidFeatureId;
    return true;
}

} // namespace nexus::app
