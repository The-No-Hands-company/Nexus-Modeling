#pragma once
// Vertex edit mode — move selected vertex to cursor position

#include <nexus/app/AppMode.h>

namespace nexus::app {

class VertexEditMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
};

} // namespace nexus::app
