#pragma once
// Face edit mode — extrude selected face along its normal

#include <nexus/app/AppMode.h>

namespace nexus::app {

class FaceEditMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
};

} // namespace nexus::app
