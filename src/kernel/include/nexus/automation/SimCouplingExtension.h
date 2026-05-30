#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — SimCouplingHarness extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register sim_coupling.register, sim_coupling.unregister, sim_coupling.sync,
/// sim_coupling.query, sim_coupling.clear, sim_coupling.describe commands.
void registerSimCouplingCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
