#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Neural renderer extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register neural.init, neural.describe commands.
void registerNeuralCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
