#pragma once
// Boolean mode — union, difference, intersection operations

#include <nexus/app/AppMode.h>

namespace nexus::app {

class BooleanMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
};

} // namespace nexus::app
