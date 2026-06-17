#pragma once
// Fillet mode — click to apply chamfer to all mesh features

#include <nexus/app/AppMode.h>

namespace nexus::app {

class FilletMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
};

} // namespace nexus::app
