#include <nexus/app/EdgeEditMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>
#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/Mesh.h>
#include <cstdio>

namespace nexus::app {

[[nodiscard]] std::string EdgeEditMode::modeId() const { return "edge-edit"; }
[[nodiscard]] std::string EdgeEditMode::displayName() const { return "Edge Edit"; }
[[nodiscard]] std::vector<ActionDescriptor> EdgeEditMode::actions() const {
    return {{"edge.bevel","Bevel","B","modify","Edge"}};
}
EventResult EdgeEditMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
    if(!ctx.document) return EventResult::Unconsumed;
    auto target = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(target);
    if(!node||!node->mesh||node->deleted) return EventResult::Unconsumed;
    geometry::Mesh saved = *node->mesh;
    geometry::BevelChamferDesc desc;
    desc.mode=geometry::BevelChamferMode::Bevel; desc.distance=0.1f; desc.sharpAngleDegrees=15.f;
    geometry::Mesh output;
    auto report = geometry::BevelChamferOperation::apply(*node->mesh, desc, output);
    if(report.isSuccess()) {
        node->mesh.emplace(std::move(output));
        auto cmd = std::make_unique<nexus::cad::TransformCommand>(target, std::move(saved));
        (void)ctx.document->executeCommand(std::move(cmd));
        printf("Bevel edge (angle>=15)\n");
    }
    return EventResult::Consumed;
}

} // namespace nexus::app
