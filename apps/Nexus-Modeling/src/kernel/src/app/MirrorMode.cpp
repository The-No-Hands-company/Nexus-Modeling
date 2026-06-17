#include <nexus/app/MirrorMode.h>
#include <nexus/cad/CadDocument.h>
#include <cstdio>

namespace nexus::app {

[[nodiscard]] std::string MirrorMode::modeId() const { return "mirror"; }
[[nodiscard]] std::string MirrorMode::displayName() const { return "Mirror"; }
EventResult MirrorMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
    if(!ctx.document) return EventResult::Unconsumed;
    auto fid = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(fid);
    if(!node||!node->mesh||node->deleted) return EventResult::Unconsumed;
    auto pos = node->mesh->attributes().positions();
    for(auto& v:pos) v.x = -v.x;
    node->mesh->attributes().setPositions(std::move(pos));
    printf("Mirror feature %u across YZ plane\n", fid);
    return EventResult::Consumed;
}

} // namespace nexus::app
