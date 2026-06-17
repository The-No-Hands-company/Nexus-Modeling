#pragma once
// Pattern mode — linear/circular pattern of selected features

#include <nexus/app/AppMode.h>

namespace nexus::app {

class PatternMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
    bool executeAction(const std::string& actionId, AppContext& ctx) override;
private:
    enum { Linear, Circular } m_mode = Linear;
};

} // namespace nexus::app
