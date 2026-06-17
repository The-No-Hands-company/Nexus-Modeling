#pragma once
// Mirror mode — mirror selected feature across plane on click

#include <nexus/app/AppMode.h>

namespace nexus::app {

class MirrorMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
};

} // namespace nexus::app
