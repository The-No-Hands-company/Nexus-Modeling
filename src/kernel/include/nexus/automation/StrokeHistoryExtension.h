#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Stroke history serialization extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register stroke_hist.serialize, stroke_hist.deserialize,
/// stroke_hist.describe commands.
void registerStrokeHistoryCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
