#pragma once
// Select mode — pick and selection operations

#include <nexus/app/AppMode.h>

namespace nexus::app {

class SelectMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
};

} // namespace nexus::app
