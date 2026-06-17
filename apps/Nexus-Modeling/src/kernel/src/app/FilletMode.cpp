#include <nexus/app/FilletMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/Mesh.h>

namespace nexus::app {

[[nodiscard]] std::string FilletMode::modeId() const { return "fillet"; }
[[nodiscard]] std::string FilletMode::displayName() const { return "Fillet"; }
[[nodiscard]] std::vector<ActionDescriptor> FilletMode::actions() const {
    return {{"fillet.constant","Constant Radius","F","modify","Fillet"},
            {"fillet.variable","Variable Radius","Shift+F","modify","Fillet"}};
}
EventResult FilletMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(event.type != InputEventType::MouseDown || event.button != 0)
        return EventResult::Unconsumed;
    if(!ctx.document) return EventResult::Unconsumed;
    auto fid = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(fid);
    if(!node || !node->mesh || node->deleted) return EventResult::Unconsumed;
    geometry::BevelChamferDesc desc;
    desc.mode = geometry::BevelChamferMode::Chamfer;
    desc.distance = 0.12f;
    desc.sharpAngleDegrees = 20.f;
    geometry::Mesh output;
    auto report = geometry::BevelChamferOperation::apply(*node->mesh, desc, output);
    if(report.isSuccess()) { node->mesh.emplace(std::move(output)); return EventResult::Consumed; }
    return EventResult::Unconsumed;
}

} // namespace nexus::app
