#include <nexus/app/DimensionMode.h>
#include <cstdio>

namespace nexus::app {

[[nodiscard]] std::string DimensionMode::modeId() const { return "dimension"; }
[[nodiscard]] std::string DimensionMode::displayName() const { return "Dimension"; }
[[nodiscard]] std::vector<ActionDescriptor> DimensionMode::actions() const {
    return {{"dim.linear","Linear Dim","D","annotate","Dimension"}};
}
EventResult DimensionMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
    if(!m_firstSet) {
        m_p1=ctx.cursorWorldPos; m_firstSet=true;
        ctx.dimP1=m_p1; ctx.dimP2=m_p1; ctx.dimActive=true;
    } else {
        m_p2=ctx.cursorWorldPos; m_firstSet=false;
        ctx.dimP2=m_p2; ctx.dimActive=true;
        float d=(m_p2-m_p1).length();
        printf("Dimension: %.2f units\n", d);
    }
    return EventResult::Consumed;
}
void DimensionMode::renderOverlay(AppContext&) {}

} // namespace nexus::app
