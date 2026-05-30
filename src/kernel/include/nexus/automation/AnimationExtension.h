#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Animation extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register anim.add_bone, anim.pose_bone, anim.compute_matrices,
/// anim.snapshot, anim.diff, anim.describe,
/// anim.build_mapping, anim.retarget commands.
void registerAnimationCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
