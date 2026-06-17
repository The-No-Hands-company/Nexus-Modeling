#include <nexus/app/SketchMode.h>

namespace nexus::app {

[[nodiscard]] std::string SketchMode::modeId() const { return "sketch"; }
[[nodiscard]] std::string SketchMode::displayName() const { return "Sketch"; }
[[nodiscard]] std::vector<ActionDescriptor> SketchMode::actions() const {
    return {{"sketch.line","Line","L","draw","Sketch"},
            {"sketch.rectangle","Rectangle","R","draw","Sketch"},
            {"sketch.circle","Circle","C","draw","Sketch"},
            {"sketch.arc","Arc","A","draw","Sketch"},
            {"sketch.polyline","Polyline","P","draw","Sketch"}};
}
EventResult SketchMode::handleInput(AppContext&, const InputEvent& event) {
    return event.type==InputEventType::MouseDown&&event.button==0
        ? EventResult::Consumed : EventResult::Unconsumed;
}
void SketchMode::renderOverlay(AppContext&) {}

} // namespace nexus::app
