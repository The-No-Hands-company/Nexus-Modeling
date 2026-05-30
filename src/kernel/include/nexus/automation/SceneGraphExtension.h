#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — SceneGraph and Camera extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register camera.set_perspective, camera.set_ortho, camera.look_at,
/// camera.describe,
/// sgraph.create_node, sgraph.remove_node, sgraph.set_transform,
/// sgraph.set_visible, sgraph.clear, sgraph.describe commands.
void registerSceneGraphCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
