#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — EvalGraph extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register eval.add_node, eval.connect, eval.disconnect, eval.evaluate,
/// eval.mark_dirty, and eval.describe commands.
void registerEvalCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
