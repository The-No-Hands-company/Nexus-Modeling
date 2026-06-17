#pragma once
// Dimension mode — two clicks to create linear dimension line in 3D

#include <nexus/app/AppMode.h>

namespace nexus::app {

class DimensionMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    void renderOverlay(AppContext& ctx) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
private:
    bool m_firstSet = false;
    Vec3 m_p1{}, m_p2{};
};

} // namespace nexus::app
