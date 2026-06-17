#include <nexus/app/SelectMode.h>

namespace nexus::app {

[[nodiscard]] std::string SelectMode::modeId() const { return "select"; }
[[nodiscard]] std::string SelectMode::displayName() const { return "Select"; }
[[nodiscard]] std::vector<ActionDescriptor> SelectMode::actions() const {
    return {{"select.pick","Pick","LMB","select","Select"},
            {"select.all","Select All","Ctrl+A","select","Select"}};
}

} // namespace nexus::app
