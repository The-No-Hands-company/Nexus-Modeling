#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Sculpt + BevelChamfer extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register sculpt.stroke, sculpt.describe, and mesh.bevel commands.
void registerSculptCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
