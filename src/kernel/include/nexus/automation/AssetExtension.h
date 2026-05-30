#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — SceneAsset extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register asset.new, asset.add_entry, asset.remove_entry,
/// asset.set_name, asset.describe,
/// gaussian.init, gaussian.add_splat, gaussian.describe commands.
void registerAssetCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
