#include <nexus/app/VertexEditMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>
#include <cstdio>

namespace nexus::app {

[[nodiscard]] std::string VertexEditMode::modeId() const { return "vertex-edit"; }
[[nodiscard]] std::string VertexEditMode::displayName() const { return "Vertex Edit"; }
[[nodiscard]] std::vector<ActionDescriptor> VertexEditMode::actions() const {
    return {{"vertex.move","Move","G","modify","Vertex"}};
}
EventResult VertexEditMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
    if(!ctx.document || ctx.selectedVertex==~0u) return EventResult::Unconsumed;
    auto target = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(target);
    if(!node||!node->mesh||node->deleted) return EventResult::Unconsumed;
    geometry::Mesh saved = *node->mesh;
    auto pos = node->mesh->attributes().positions();
    if(ctx.selectedVertex < pos.size()) {
        pos[ctx.selectedVertex] = ctx.cursorWorldPos;
        node->mesh->attributes().setPositions(std::move(pos));
        auto cmd = std::make_unique<nexus::cad::TransformCommand>(target, std::move(saved));
        (void)ctx.document->executeCommand(std::move(cmd));
        printf("Moved vertex %u\n", ctx.selectedVertex);
    }
    return EventResult::Consumed;
}

} // namespace nexus::app
