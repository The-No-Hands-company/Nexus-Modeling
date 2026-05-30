#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Remesh/mesh-processing extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register mesh.remesh and mesh.weld commands into the harness.
void registerRemeshCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
