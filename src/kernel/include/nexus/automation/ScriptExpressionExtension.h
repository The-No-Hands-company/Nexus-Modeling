#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Script Expression DSL extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register expr_dsl.compile, expr_dsl.evaluate, expr_dsl.vars,
/// expr_dsl.describe, expr_dsl.clear commands.
void registerScriptExpressionCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
