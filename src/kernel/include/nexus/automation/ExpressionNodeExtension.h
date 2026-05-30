#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — ExpressionNode extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register expr.add_constant, expr.add_adapter, expr.evaluate,
/// expr.read, and expr.describe commands.
void registerExpressionNodeCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
