#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Simulation extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register sim.rigid_init, sim.rigid_add_body, sim.rigid_step,
/// sim.rigid_apply_force, sim.rigid_snapshot, sim.rigid_diff,
/// sim.rigid_describe,
/// sim.cloth_init, sim.cloth_add_node, sim.cloth_step, sim.cloth_describe,
/// sim.fluid_init, sim.fluid_add_particle, sim.fluid_step, sim.fluid_describe.
void registerSimCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
