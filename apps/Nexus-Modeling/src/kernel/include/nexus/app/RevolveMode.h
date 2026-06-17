#pragma once
// Revolve mode — click and drag to set angle, release to create revolution

#include <nexus/app/AppMode.h>
#include <nexus/parametric/FeatureHistory.h>

namespace nexus::app {

class RevolveMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    bool onDeactivate(AppContext& ctx) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
private:
    parametric::FeatureId m_sketchId = parametric::kInvalidFeatureId;
    bool m_dragging = false;
    float m_startY = 0;
};

} // namespace nexus::app
