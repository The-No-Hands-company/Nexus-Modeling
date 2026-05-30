#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Parametric sample generator extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register param_samples.make_rect, param_samples.solve,
/// param_samples.describe commands.
void registerParametricSamplesCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
