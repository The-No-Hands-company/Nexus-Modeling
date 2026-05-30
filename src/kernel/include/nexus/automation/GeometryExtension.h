#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Geometry operation extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register mesh.extrude and mesh.inset commands into the harness.
void registerGeometryCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
