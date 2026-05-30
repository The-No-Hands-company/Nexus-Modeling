#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — NodeScene extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register scene.add_node, scene.connect, scene.disconnect,
/// scene.set_parent, scene.clear_parent, scene.evaluate,
/// and scene.describe commands.
void registerNodeSceneCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
