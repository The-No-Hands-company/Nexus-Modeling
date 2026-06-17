#include <nexus/app/BooleanMode.h>

namespace nexus::app {

[[nodiscard]] std::string BooleanMode::modeId() const { return "boolean"; }
[[nodiscard]] std::string BooleanMode::displayName() const { return "Boolean"; }
[[nodiscard]] std::vector<ActionDescriptor> BooleanMode::actions() const {
    return {{"bool.union","Union","Shift+U","modify","Boolean"},
            {"bool.diff","Difference","Shift+D","modify","Boolean"},
            {"bool.isect","Intersection","Shift+I","modify","Boolean"}};
}

} // namespace nexus::app
