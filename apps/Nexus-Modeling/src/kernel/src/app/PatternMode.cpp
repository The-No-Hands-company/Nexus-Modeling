#include <nexus/app/PatternMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>
#include <nexus/parametric/ParametricSketchProfile.h>

#include <cmath>
#include <cstdio>

namespace nexus::app {

[[nodiscard]] std::string PatternMode::modeId() const { return "pattern"; }
[[nodiscard]] std::string PatternMode::displayName() const { return "Pattern"; }
[[nodiscard]] std::vector<ActionDescriptor> PatternMode::actions() const {
    return {{"pattern.linear","Linear Array","L","create","Pattern"},
            {"pattern.circular","Circular Array","C","create","Pattern"}};
}

EventResult PatternMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if(event.type!=InputEventType::MouseDown||event.button!=0) return EventResult::Unconsumed;
    if(!ctx.document) return EventResult::Unconsumed;
    auto fid = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(fid);
    if(!node||!node->mesh||node->deleted) return EventResult::Unconsumed;

    const int count = 4;
    const float spacing = 2.5f;
    const float angleStep = 90.f;

    if(m_mode == Linear) {
        for(int i=1; i<count; ++i) {
            geometry::Mesh copy = *node->mesh;
            auto pos = copy.attributes().positions();
            for(auto& v : pos) { v.x += spacing * i; }
            copy.attributes().setPositions(std::move(pos));
            auto sk = parametric::ParametricSketchFactory::createSketch();
            auto newId = ctx.document->addSketch(sk);
            auto* newNode = ctx.document->history().node(newId);
            if(newNode) { newNode->mesh.emplace(std::move(copy)); newNode->dirty=false; }
        }
        printf("Linear array: %d copies (spacing %.1f)\n", count-1, spacing);
    } else {
        auto center = node->mesh->computeBounds().center();
        for(int i=1; i<count; ++i) {
            float angle = angleStep * i * 3.14159265f / 180.f;
            float c = std::cos(angle), s = std::sin(angle);
            geometry::Mesh copy = *node->mesh;
            auto pos = copy.attributes().positions();
            for(auto& v : pos) {
                float rx = v.x - center.x;
                float rz = v.z - center.z;
                v.x = center.x + rx*c - rz*s;
                v.z = center.z + rx*s + rz*c;
            }
            copy.attributes().setPositions(std::move(pos));
            auto sk = parametric::ParametricSketchFactory::createSketch();
            auto newId = ctx.document->addSketch(sk);
            auto* newNode = ctx.document->history().node(newId);
            if(newNode) { newNode->mesh.emplace(std::move(copy)); newNode->dirty=false; }
        }
        printf("Circular array: %d copies (%.0f deg step)\n", count-1, angleStep);
    }
    return EventResult::Consumed;
}

bool PatternMode::executeAction(const std::string& actionId, AppContext&) {
    if(actionId == "pattern.linear") m_mode = Linear;
    else if(actionId == "pattern.circular") m_mode = Circular;
    return true;
}

} // namespace nexus::app
